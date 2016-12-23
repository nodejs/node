// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/ic/handler-compiler.h"

#include "src/api-arguments.h"
#include "src/field-type.h"
#include "src/ic/call-optimization.h"
#include "src/ic/ic.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


void NamedLoadHandlerCompiler::GenerateLoadViaGetter(
    MacroAssembler* masm, Handle<Map> map, Register receiver, Register holder,
    int accessor_index, int expected_arguments, Register scratch) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save context register
    __ push(esi);

    if (accessor_index >= 0) {
      DCHECK(!holder.is(scratch));
      DCHECK(!receiver.is(scratch));
      // Call the JavaScript getter with the receiver on the stack.
      if (map->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ mov(scratch,
               FieldOperand(receiver, JSGlobalObject::kGlobalProxyOffset));
        receiver = scratch;
      }
      __ push(receiver);
      __ LoadAccessor(edi, holder, accessor_index, ACCESSOR_GETTER);
      __ Set(eax, 0);
      __ Call(masm->isolate()->builtins()->CallFunction(
                  ConvertReceiverMode::kNotNullOrUndefined),
              RelocInfo::CODE_TARGET);
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetGetterStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context register.
    __ pop(esi);
  }
  __ ret(0);
}


void PropertyHandlerCompiler::PushVectorAndSlot(Register vector,
                                                Register slot) {
  MacroAssembler* masm = this->masm();
  STATIC_ASSERT(LoadWithVectorDescriptor::kSlot <
                LoadWithVectorDescriptor::kVector);
  STATIC_ASSERT(StoreWithVectorDescriptor::kSlot <
                StoreWithVectorDescriptor::kVector);
  STATIC_ASSERT(StoreTransitionDescriptor::kSlot <
                StoreTransitionDescriptor::kVector);
  __ push(slot);
  __ push(vector);
}


void PropertyHandlerCompiler::PopVectorAndSlot(Register vector, Register slot) {
  MacroAssembler* masm = this->masm();
  __ pop(vector);
  __ pop(slot);
}


void PropertyHandlerCompiler::DiscardVectorAndSlot() {
  MacroAssembler* masm = this->masm();
  // Remove vector and slot.
  __ add(esp, Immediate(2 * kPointerSize));
}

void PropertyHandlerCompiler::PushReturnAddress(Register tmp) {
  MacroAssembler* masm = this->masm();
  __ push(tmp);
}

void PropertyHandlerCompiler::PopReturnAddress(Register tmp) {
  MacroAssembler* masm = this->masm();
  __ pop(tmp);
}

void PropertyHandlerCompiler::GenerateDictionaryNegativeLookup(
    MacroAssembler* masm, Label* miss_label, Register receiver,
    Handle<Name> name, Register scratch0, Register scratch1) {
  DCHECK(name->IsUniqueName());
  DCHECK(!receiver.is(scratch0));
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->negative_lookups(), 1);
  __ IncrementCounter(counters->negative_lookups_miss(), 1);

  __ mov(scratch0, FieldOperand(receiver, HeapObject::kMapOffset));

  const int kInterceptorOrAccessCheckNeededMask =
      (1 << Map::kHasNamedInterceptor) | (1 << Map::kIsAccessCheckNeeded);

  // Bail out if the receiver has a named interceptor or requires access checks.
  __ test_b(FieldOperand(scratch0, Map::kBitFieldOffset),
            Immediate(kInterceptorOrAccessCheckNeededMask));
  __ j(not_zero, miss_label);

  // Check that receiver is a JSObject.
  __ CmpInstanceType(scratch0, FIRST_JS_RECEIVER_TYPE);
  __ j(below, miss_label);

  // Load properties array.
  Register properties = scratch0;
  __ mov(properties, FieldOperand(receiver, JSObject::kPropertiesOffset));

  // Check that the properties array is a dictionary.
  __ cmp(FieldOperand(properties, HeapObject::kMapOffset),
         Immediate(masm->isolate()->factory()->hash_table_map()));
  __ j(not_equal, miss_label);

  Label done;
  NameDictionaryLookupStub::GenerateNegativeLookup(masm, miss_label, &done,
                                                   properties, name, scratch1);
  __ bind(&done);
  __ DecrementCounter(counters->negative_lookups_miss(), 1);
}


void NamedLoadHandlerCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm, int index, Register result, Label* miss) {
  __ LoadGlobalFunction(index, result);
  // Load its initial map. The global functions all have initial maps.
  __ mov(result,
         FieldOperand(result, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the prototype from the initial map.
  __ mov(result, FieldOperand(result, Map::kPrototypeOffset));
}


void NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(
    MacroAssembler* masm, Register receiver, Register scratch1,
    Register scratch2, Label* miss_label) {
  // TODO(mvstanton): This isn't used on ia32. Move all the other
  // platform implementations into a code stub so this method can be removed.
  UNREACHABLE();
}


// Generate call to api function.
// This function uses push() to generate smaller, faster code than
// the version above. It is an optimization that should will be removed
// when api call ICs are generated in hydrogen.
void PropertyHandlerCompiler::GenerateApiAccessorCall(
    MacroAssembler* masm, const CallOptimization& optimization,
    Handle<Map> receiver_map, Register receiver, Register scratch,
    bool is_store, Register store_parameter, Register accessor_holder,
    int accessor_index) {
  DCHECK(!accessor_holder.is(scratch));
  // Copy return value.
  __ pop(scratch);

  if (is_store) {
    // Discard stack arguments.
    __ add(esp, Immediate(StoreWithVectorDescriptor::kStackArgumentsCount *
                          kPointerSize));
  }
  // Write the receiver and arguments to stack frame.
  __ push(receiver);
  if (is_store) {
    DCHECK(!AreAliased(receiver, scratch, store_parameter));
    __ push(store_parameter);
  }
  __ push(scratch);
  // Stack now matches JSFunction abi.
  DCHECK(optimization.is_simple_api_call());

  // Abi for CallApiCallbackStub.
  Register callee = edi;
  Register data = ebx;
  Register holder = ecx;
  Register api_function_address = edx;
  scratch = no_reg;

  // Put callee in place.
  __ LoadAccessor(callee, accessor_holder, accessor_index,
                  is_store ? ACCESSOR_SETTER : ACCESSOR_GETTER);

  // Put holder in place.
  CallOptimization::HolderLookup holder_lookup;
  int holder_depth = 0;
  optimization.LookupHolderOfExpectedType(receiver_map, &holder_lookup,
                                          &holder_depth);
  switch (holder_lookup) {
    case CallOptimization::kHolderIsReceiver:
      __ Move(holder, receiver);
      break;
    case CallOptimization::kHolderFound:
      __ mov(holder, FieldOperand(receiver, HeapObject::kMapOffset));
      __ mov(holder, FieldOperand(holder, Map::kPrototypeOffset));
      for (int i = 1; i < holder_depth; i++) {
        __ mov(holder, FieldOperand(holder, HeapObject::kMapOffset));
        __ mov(holder, FieldOperand(holder, Map::kPrototypeOffset));
      }
      break;
    case CallOptimization::kHolderNotFound:
      UNREACHABLE();
      break;
  }

  Isolate* isolate = masm->isolate();
  Handle<CallHandlerInfo> api_call_info = optimization.api_call_info();
  bool call_data_undefined = false;
  // Put call data in place.
  if (api_call_info->data()->IsUndefined(isolate)) {
    call_data_undefined = true;
    __ mov(data, Immediate(isolate->factory()->undefined_value()));
  } else {
    if (optimization.is_constant_call()) {
      __ mov(data, FieldOperand(callee, JSFunction::kSharedFunctionInfoOffset));
      __ mov(data, FieldOperand(data, SharedFunctionInfo::kFunctionDataOffset));
      __ mov(data, FieldOperand(data, FunctionTemplateInfo::kCallCodeOffset));
    } else {
      __ mov(data, FieldOperand(callee, FunctionTemplateInfo::kCallCodeOffset));
    }
    __ mov(data, FieldOperand(data, CallHandlerInfo::kDataOffset));
  }

  if (api_call_info->fast_handler()->IsCode()) {
    // Just tail call into the code.
    __ Jump(handle(Code::cast(api_call_info->fast_handler())),
            RelocInfo::CODE_TARGET);
    return;
  }
  // Put api_function_address in place.
  Address function_address = v8::ToCData<Address>(api_call_info->callback());
  __ mov(api_function_address, Immediate(function_address));

  // Jump to stub.
  CallApiCallbackStub stub(isolate, is_store, call_data_undefined,
                           !optimization.is_constant_call());
  __ TailCallStub(&stub);
}


// Generate code to check that a global property cell is empty. Create
// the property cell at compilation time if no cell exists for the
// property.
void PropertyHandlerCompiler::GenerateCheckPropertyCell(
    MacroAssembler* masm, Handle<JSGlobalObject> global, Handle<Name> name,
    Register scratch, Label* miss) {
  Handle<PropertyCell> cell = JSGlobalObject::EnsureEmptyPropertyCell(
      global, name, PropertyCellType::kInvalidated);
  Isolate* isolate = masm->isolate();
  DCHECK(cell->value()->IsTheHole(isolate));
  Handle<WeakCell> weak_cell = isolate->factory()->NewWeakCell(cell);
  __ LoadWeakValue(scratch, weak_cell, miss);
  __ cmp(FieldOperand(scratch, PropertyCell::kValueOffset),
         Immediate(isolate->factory()->the_hole_value()));
  __ j(not_equal, miss);
}


void NamedStoreHandlerCompiler::GenerateStoreViaSetter(
    MacroAssembler* masm, Handle<Map> map, Register receiver, Register holder,
    int accessor_index, int expected_arguments, Register scratch) {
  // ----------- S t a t e -------------
  //  -- esp[12] : value
  //  -- esp[8]  : slot
  //  -- esp[4]  : vector
  //  -- esp[0]  : return address
  // -----------------------------------
  __ LoadParameterFromStack<Descriptor>(value(), Descriptor::kValue);

  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save context register
    __ push(esi);
    // Save value register, so we can restore it later.
    __ push(value());

    if (accessor_index >= 0) {
      DCHECK(!holder.is(scratch));
      DCHECK(!receiver.is(scratch));
      DCHECK(!value().is(scratch));
      // Call the JavaScript setter with receiver and value on the stack.
      if (map->IsJSGlobalObjectMap()) {
        __ mov(scratch,
               FieldOperand(receiver, JSGlobalObject::kGlobalProxyOffset));
        receiver = scratch;
      }
      __ push(receiver);
      __ push(value());
      __ LoadAccessor(edi, holder, accessor_index, ACCESSOR_SETTER);
      __ Set(eax, 1);
      __ Call(masm->isolate()->builtins()->CallFunction(
                  ConvertReceiverMode::kNotNullOrUndefined),
              RelocInfo::CODE_TARGET);
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetSetterStubDeoptPCOffset(masm->pc_offset());
    }

    // We have to return the passed value, not the return value of the setter.
    __ pop(eax);
    // Restore context register.
    __ pop(esi);
  }
  if (accessor_index >= 0) {
    __ ret(StoreWithVectorDescriptor::kStackArgumentsCount * kPointerSize);
  } else {
    // If we generate a global code snippet for deoptimization only, don't try
    // to drop stack arguments for the StoreIC because they are not a part of
    // expression stack and deoptimizer does not reconstruct them.
    __ ret(0);
  }
}


static void PushInterceptorArguments(MacroAssembler* masm, Register receiver,
                                     Register holder, Register name,
                                     Handle<JSObject> holder_obj) {
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsNameIndex == 0);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsThisIndex == 1);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsHolderIndex == 2);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsLength == 3);
  __ push(name);
  __ push(receiver);
  __ push(holder);
}


static void CompileCallLoadPropertyWithInterceptor(
    MacroAssembler* masm, Register receiver, Register holder, Register name,
    Handle<JSObject> holder_obj, Runtime::FunctionId id) {
  DCHECK(NamedLoadHandlerCompiler::kInterceptorArgsLength ==
         Runtime::FunctionForId(id)->nargs);
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);
  __ CallRuntime(id);
}

#undef __
#define __ ACCESS_MASM(masm())


void NamedStoreHandlerCompiler::GenerateRestoreName(Label* label,
                                                    Handle<Name> name) {
  if (!label->is_unused()) {
    __ bind(label);
    __ mov(this->name(), Immediate(name));
  }
}


void NamedStoreHandlerCompiler::GenerateRestoreName(Handle<Name> name) {
  __ mov(this->name(), Immediate(name));
}


void NamedStoreHandlerCompiler::GenerateRestoreMap(Handle<Map> transition,
                                                   Register map_reg,
                                                   Register scratch,
                                                   Label* miss) {
  Handle<WeakCell> cell = Map::WeakCellForMap(transition);
  DCHECK(!map_reg.is(scratch));
  __ LoadWeakValue(map_reg, cell, miss);
  if (transition->CanBeDeprecated()) {
    __ mov(scratch, FieldOperand(map_reg, Map::kBitField3Offset));
    __ and_(scratch, Immediate(Map::Deprecated::kMask));
    __ j(not_zero, miss);
  }
}


void NamedStoreHandlerCompiler::GenerateConstantCheck(Register map_reg,
                                                      int descriptor,
                                                      Register value_reg,
                                                      Register scratch,
                                                      Label* miss_label) {
  DCHECK(!map_reg.is(scratch));
  DCHECK(!map_reg.is(value_reg));
  DCHECK(!value_reg.is(scratch));
  __ LoadInstanceDescriptors(map_reg, scratch);
  __ mov(scratch,
         FieldOperand(scratch, DescriptorArray::GetValueOffset(descriptor)));
  __ cmp(value_reg, scratch);
  __ j(not_equal, miss_label);
}

void NamedStoreHandlerCompiler::GenerateFieldTypeChecks(FieldType* field_type,
                                                        Register value_reg,
                                                        Label* miss_label) {
  Register map_reg = scratch1();
  Register scratch = scratch2();
  DCHECK(!value_reg.is(map_reg));
  DCHECK(!value_reg.is(scratch));
  __ JumpIfSmi(value_reg, miss_label);
  if (field_type->IsClass()) {
    __ mov(map_reg, FieldOperand(value_reg, HeapObject::kMapOffset));
    __ CmpWeakValue(map_reg, Map::WeakCellForMap(field_type->AsClass()),
                    scratch);
    __ j(not_equal, miss_label);
  }
}


Register PropertyHandlerCompiler::CheckPrototypes(
    Register object_reg, Register holder_reg, Register scratch1,
    Register scratch2, Handle<Name> name, Label* miss, PrototypeCheckType check,
    ReturnHolder return_what) {
  Handle<Map> receiver_map = map();

  // Make sure there's no overlap between holder and object registers.
  DCHECK(!scratch1.is(object_reg) && !scratch1.is(holder_reg));
  DCHECK(!scratch2.is(object_reg) && !scratch2.is(holder_reg) &&
         !scratch2.is(scratch1));

  Handle<Cell> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate());
  if (!validity_cell.is_null()) {
    DCHECK_EQ(Smi::FromInt(Map::kPrototypeChainValid), validity_cell->value());
    // Operand::ForCell(...) points to the cell's payload!
    __ cmp(Operand::ForCell(validity_cell),
           Immediate(Smi::FromInt(Map::kPrototypeChainValid)));
    __ j(not_equal, miss);
  }

  // The prototype chain of primitives (and their JSValue wrappers) depends
  // on the native context, which can't be guarded by validity cells.
  // |object_reg| holds the native context specific prototype in this case;
  // we need to check its map.
  if (check == CHECK_ALL_MAPS) {
    __ mov(scratch1, FieldOperand(object_reg, HeapObject::kMapOffset));
    Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);
    __ CmpWeakValue(scratch1, cell, scratch2);
    __ j(not_equal, miss);
  }

  // Keep track of the current object in register reg.
  Register reg = object_reg;
  int depth = 0;

  Handle<JSObject> current = Handle<JSObject>::null();
  if (receiver_map->IsJSGlobalObjectMap()) {
    current = isolate()->global_object();
  }

  // Check access rights to the global object.  This has to happen after
  // the map check so that we know that the object is actually a global
  // object.
  // This allows us to install generated handlers for accesses to the
  // global proxy (as opposed to using slow ICs). See corresponding code
  // in LookupForRead().
  if (receiver_map->IsJSGlobalProxyMap()) {
    __ CheckAccessGlobalProxy(reg, scratch1, scratch2, miss);
  }

  Handle<JSObject> prototype = Handle<JSObject>::null();
  Handle<Map> current_map = receiver_map;
  Handle<Map> holder_map(holder()->map());
  // Traverse the prototype chain and check the maps in the prototype chain for
  // fast and global objects or do negative lookup for normal objects.
  while (!current_map.is_identical_to(holder_map)) {
    ++depth;

    // Only global objects and objects that do not require access
    // checks are allowed in stubs.
    DCHECK(current_map->IsJSGlobalProxyMap() ||
           !current_map->is_access_check_needed());

    prototype = handle(JSObject::cast(current_map->prototype()));
    if (current_map->IsJSGlobalObjectMap()) {
      GenerateCheckPropertyCell(masm(), Handle<JSGlobalObject>::cast(current),
                                name, scratch2, miss);
    } else if (current_map->is_dictionary_map()) {
      DCHECK(!current_map->IsJSGlobalProxyMap());  // Proxy maps are fast.
      if (!name->IsUniqueName()) {
        DCHECK(name->IsString());
        name = factory()->InternalizeString(Handle<String>::cast(name));
      }
      DCHECK(current.is_null() ||
             current->property_dictionary()->FindEntry(name) ==
                 NameDictionary::kNotFound);

      if (depth > 1) {
        // TODO(jkummerow): Cache and re-use weak cell.
        __ LoadWeakValue(reg, isolate()->factory()->NewWeakCell(current), miss);
      }
      GenerateDictionaryNegativeLookup(masm(), miss, reg, name, scratch1,
                                       scratch2);
    }

    reg = holder_reg;  // From now on the object will be in holder_reg.
    // Go to the next object in the prototype chain.
    current = prototype;
    current_map = handle(current->map());
  }

  DCHECK(!current_map->IsJSGlobalProxyMap());

  // Log the check depth.
  LOG(isolate(), IntEvent("check-maps-depth", depth + 1));

  bool return_holder = return_what == RETURN_HOLDER;
  if (return_holder && depth != 0) {
    __ LoadWeakValue(reg, isolate()->factory()->NewWeakCell(current), miss);
  }

  // Return the register containing the holder.
  return return_holder ? reg : no_reg;
}


void NamedLoadHandlerCompiler::FrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ jmp(&success);
    __ bind(miss);
    if (IC::ShouldPushPopSlotAndVector(kind())) {
      DCHECK(kind() == Code::LOAD_IC);
      PopVectorAndSlot();
    }
    TailCallBuiltin(masm(), MissBuiltin(kind()));
    __ bind(&success);
  }
}


void NamedStoreHandlerCompiler::FrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ jmp(&success);
    GenerateRestoreName(miss, name);
    DCHECK(!IC::ShouldPushPopSlotAndVector(kind()));
    TailCallBuiltin(masm(), MissBuiltin(kind()));
    __ bind(&success);
  }
}


void NamedLoadHandlerCompiler::GenerateLoadConstant(Handle<Object> value) {
  // Return the constant value.
  __ LoadObject(eax, value);
  __ ret(0);
}


void NamedLoadHandlerCompiler::GenerateLoadInterceptorWithFollowup(
    LookupIterator* it, Register holder_reg) {
  DCHECK(holder()->HasNamedInterceptor());
  DCHECK(!holder()->GetNamedInterceptor()->getter()->IsUndefined(isolate()));

  // Compile the interceptor call, followed by inline code to load the
  // property from further up the prototype chain if the call fails.
  // Check that the maps haven't changed.
  DCHECK(holder_reg.is(receiver()) || holder_reg.is(scratch1()));

  // Preserve the receiver register explicitly whenever it is different from the
  // holder and it is needed should the interceptor return without any result.
  // The ACCESSOR case needs the receiver to be passed into C++ code, the FIELD
  // case might cause a miss during the prototype check.
  bool must_perform_prototype_check =
      !holder().is_identical_to(it->GetHolder<JSObject>());
  bool must_preserve_receiver_reg =
      !receiver().is(holder_reg) &&
      (it->state() == LookupIterator::ACCESSOR || must_perform_prototype_check);

  // Save necessary data before invoking an interceptor.
  // Requires a frame to make GC aware of pushed pointers.
  {
    FrameScope frame_scope(masm(), StackFrame::INTERNAL);

    if (must_preserve_receiver_reg) {
      __ push(receiver());
    }
    __ push(holder_reg);
    __ push(this->name());
    InterceptorVectorSlotPush(holder_reg);
    // Invoke an interceptor.  Note: map checks from receiver to
    // interceptor's holder has been compiled before (see a caller
    // of this method.)
    CompileCallLoadPropertyWithInterceptor(
        masm(), receiver(), holder_reg, this->name(), holder(),
        Runtime::kLoadPropertyWithInterceptorOnly);

    // Check if interceptor provided a value for property.  If it's
    // the case, return immediately.
    Label interceptor_failed;
    __ cmp(eax, factory()->no_interceptor_result_sentinel());
    __ j(equal, &interceptor_failed);
    frame_scope.GenerateLeaveFrame();
    __ ret(0);

    // Clobber registers when generating debug-code to provoke errors.
    __ bind(&interceptor_failed);
    if (FLAG_debug_code) {
      __ mov(receiver(), Immediate(bit_cast<int32_t>(kZapValue)));
      __ mov(holder_reg, Immediate(bit_cast<int32_t>(kZapValue)));
      __ mov(this->name(), Immediate(bit_cast<int32_t>(kZapValue)));
    }

    InterceptorVectorSlotPop(holder_reg);
    __ pop(this->name());
    __ pop(holder_reg);
    if (must_preserve_receiver_reg) {
      __ pop(receiver());
    }

    // Leave the internal frame.
  }

  GenerateLoadPostInterceptor(it, holder_reg);
}


void NamedLoadHandlerCompiler::GenerateLoadInterceptor(Register holder_reg) {
  DCHECK(holder()->HasNamedInterceptor());
  DCHECK(!holder()->GetNamedInterceptor()->getter()->IsUndefined(isolate()));
  // Call the runtime system to load the interceptor.
  __ pop(scratch2());  // save old return address
  PushInterceptorArguments(masm(), receiver(), holder_reg, this->name(),
                           holder());
  __ push(scratch2());  // restore old return address

  __ TailCallRuntime(Runtime::kLoadPropertyWithInterceptor);
}

void NamedStoreHandlerCompiler::ZapStackArgumentsRegisterAliases() {
  // Zap register aliases of the arguments passed on the stack to ensure they
  // are properly loaded by the handler (debug-only).
  STATIC_ASSERT(Descriptor::kPassLastArgsOnStack);
  STATIC_ASSERT(Descriptor::kStackArgumentsCount == 3);
  __ mov(Descriptor::ValueRegister(), Immediate(kDebugZapValue));
  __ mov(Descriptor::SlotRegister(), Immediate(kDebugZapValue));
  __ mov(Descriptor::VectorRegister(), Immediate(kDebugZapValue));
}

Handle<Code> NamedStoreHandlerCompiler::CompileStoreCallback(
    Handle<JSObject> object, Handle<Name> name, Handle<AccessorInfo> callback,
    LanguageMode language_mode) {
  Register holder_reg = Frontend(name);
  __ LoadParameterFromStack<Descriptor>(value(), Descriptor::kValue);

  __ pop(scratch1());  // remove the return address
  // Discard stack arguments.
  __ add(esp, Immediate(StoreWithVectorDescriptor::kStackArgumentsCount *
                        kPointerSize));
  __ push(receiver());
  __ push(holder_reg);
  // If the callback cannot leak, then push the callback directly,
  // otherwise wrap it in a weak cell.
  if (callback->data()->IsUndefined(isolate()) || callback->data()->IsSmi()) {
    __ Push(callback);
  } else {
    Handle<WeakCell> cell = isolate()->factory()->NewWeakCell(callback);
    __ Push(cell);
  }
  __ Push(name);
  __ push(value());
  __ push(Immediate(Smi::FromInt(language_mode)));
  __ push(scratch1());  // restore return address

  // Do tail-call to the runtime system.
  __ TailCallRuntime(Runtime::kStoreCallbackProperty);

  // Return the generated code.
  return GetCode(kind(), name);
}


Register NamedStoreHandlerCompiler::value() {
  return StoreDescriptor::ValueRegister();
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadGlobal(
    Handle<PropertyCell> cell, Handle<Name> name, bool is_configurable) {
  Label miss;
  if (IC::ShouldPushPopSlotAndVector(kind())) {
    PushVectorAndSlot();
  }
  FrontendHeader(receiver(), name, &miss, DONT_RETURN_ANYTHING);
  // Get the value from the cell.
  Register result = StoreDescriptor::ValueRegister();
  Handle<WeakCell> weak_cell = factory()->NewWeakCell(cell);
  __ LoadWeakValue(result, weak_cell, &miss);
  __ mov(result, FieldOperand(result, PropertyCell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (is_configurable) {
    __ cmp(result, factory()->the_hole_value());
    __ j(equal, &miss);
  } else if (FLAG_debug_code) {
    __ cmp(result, factory()->the_hole_value());
    __ Check(not_equal, kDontDeleteCellsCannotContainTheHole);
  }

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->ic_named_load_global_stub(), 1);
  // The code above already loads the result into the return register.
  if (IC::ShouldPushPopSlotAndVector(kind())) {
    DiscardVectorAndSlot();
  }
  __ ret(0);

  FrontendFooter(name, &miss);

  // Return the generated code.
  return GetCode(kind(), name);
}


#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32
