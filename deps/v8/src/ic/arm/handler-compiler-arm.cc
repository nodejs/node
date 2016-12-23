// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

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
  // ----------- S t a t e -------------
  //  -- r0    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

    // Save context register
    __ push(cp);

    if (accessor_index >= 0) {
      DCHECK(!holder.is(scratch));
      DCHECK(!receiver.is(scratch));
      // Call the JavaScript getter with the receiver on the stack.
      if (map->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ ldr(scratch,
               FieldMemOperand(receiver, JSGlobalObject::kGlobalProxyOffset));
        receiver = scratch;
      }
      __ push(receiver);
      __ LoadAccessor(r1, holder, accessor_index, ACCESSOR_GETTER);
      __ mov(r0, Operand(0));
      __ Call(masm->isolate()->builtins()->CallFunction(
                  ConvertReceiverMode::kNotNullOrUndefined),
              RelocInfo::CODE_TARGET);
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetGetterStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context register.
    __ pop(cp);
  }
  __ Ret();
}


void NamedStoreHandlerCompiler::GenerateStoreViaSetter(
    MacroAssembler* masm, Handle<Map> map, Register receiver, Register holder,
    int accessor_index, int expected_arguments, Register scratch) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  // -----------------------------------
  {
    FrameAndConstantPoolScope scope(masm, StackFrame::INTERNAL);

    // Save context register
    __ push(cp);
    // Save value register, so we can restore it later.
    __ push(value());

    if (accessor_index >= 0) {
      DCHECK(!holder.is(scratch));
      DCHECK(!receiver.is(scratch));
      DCHECK(!value().is(scratch));
      // Call the JavaScript setter with receiver and value on the stack.
      if (map->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ ldr(scratch,
               FieldMemOperand(receiver, JSGlobalObject::kGlobalProxyOffset));
        receiver = scratch;
      }
      __ Push(receiver, value());
      __ LoadAccessor(r1, holder, accessor_index, ACCESSOR_SETTER);
      __ mov(r0, Operand(1));
      __ Call(masm->isolate()->builtins()->CallFunction(
                  ConvertReceiverMode::kNotNullOrUndefined),
              RelocInfo::CODE_TARGET);
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetSetterStubDeoptPCOffset(masm->pc_offset());
    }

    // We have to return the passed value, not the return value of the setter.
    __ pop(r0);

    // Restore context register.
    __ pop(cp);
  }
  __ Ret();
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
  __ add(sp, sp, Operand(2 * kPointerSize));
}

void PropertyHandlerCompiler::PushReturnAddress(Register tmp) {
  // No-op. Return address is in lr register.
}

void PropertyHandlerCompiler::PopReturnAddress(Register tmp) {
  // No-op. Return address is in lr register.
}

void PropertyHandlerCompiler::GenerateDictionaryNegativeLookup(
    MacroAssembler* masm, Label* miss_label, Register receiver,
    Handle<Name> name, Register scratch0, Register scratch1) {
  DCHECK(name->IsUniqueName());
  DCHECK(!receiver.is(scratch0));
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->negative_lookups(), 1, scratch0, scratch1);
  __ IncrementCounter(counters->negative_lookups_miss(), 1, scratch0, scratch1);

  Label done;

  const int kInterceptorOrAccessCheckNeededMask =
      (1 << Map::kHasNamedInterceptor) | (1 << Map::kIsAccessCheckNeeded);

  // Bail out if the receiver has a named interceptor or requires access checks.
  Register map = scratch1;
  __ ldr(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ ldrb(scratch0, FieldMemOperand(map, Map::kBitFieldOffset));
  __ tst(scratch0, Operand(kInterceptorOrAccessCheckNeededMask));
  __ b(ne, miss_label);

  // Check that receiver is a JSObject.
  __ ldrb(scratch0, FieldMemOperand(map, Map::kInstanceTypeOffset));
  __ cmp(scratch0, Operand(FIRST_JS_RECEIVER_TYPE));
  __ b(lt, miss_label);

  // Load properties array.
  Register properties = scratch0;
  __ ldr(properties, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  // Check that the properties array is a dictionary.
  __ ldr(map, FieldMemOperand(properties, HeapObject::kMapOffset));
  Register tmp = properties;
  __ LoadRoot(tmp, Heap::kHashTableMapRootIndex);
  __ cmp(map, tmp);
  __ b(ne, miss_label);

  // Restore the temporarily used register.
  __ ldr(properties, FieldMemOperand(receiver, JSObject::kPropertiesOffset));


  NameDictionaryLookupStub::GenerateNegativeLookup(
      masm, miss_label, &done, receiver, properties, name, scratch1);
  __ bind(&done);
  __ DecrementCounter(counters->negative_lookups_miss(), 1, scratch0, scratch1);
}


void NamedLoadHandlerCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm, int index, Register result, Label* miss) {
  __ LoadNativeContextSlot(index, result);
  // Load its initial map. The global functions all have initial maps.
  __ ldr(result,
         FieldMemOperand(result, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the prototype from the initial map.
  __ ldr(result, FieldMemOperand(result, Map::kPrototypeOffset));
}


void NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(
    MacroAssembler* masm, Register receiver, Register scratch1,
    Register scratch2, Label* miss_label) {
  __ TryGetFunctionPrototype(receiver, scratch1, scratch2, miss_label);
  __ mov(r0, scratch1);
  __ Ret();
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
  __ ldr(scratch, FieldMemOperand(scratch, PropertyCell::kValueOffset));
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ cmp(scratch, ip);
  __ b(ne, miss);
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


// Generate call to api function.
void PropertyHandlerCompiler::GenerateApiAccessorCall(
    MacroAssembler* masm, const CallOptimization& optimization,
    Handle<Map> receiver_map, Register receiver, Register scratch_in,
    bool is_store, Register store_parameter, Register accessor_holder,
    int accessor_index) {
  DCHECK(!accessor_holder.is(scratch_in));
  DCHECK(!receiver.is(scratch_in));
  __ push(receiver);
  // Write the arguments to stack frame.
  if (is_store) {
    DCHECK(!receiver.is(store_parameter));
    DCHECK(!scratch_in.is(store_parameter));
    __ push(store_parameter);
  }
  DCHECK(optimization.is_simple_api_call());

  // Abi for CallApiCallbackStub.
  Register callee = r0;
  Register data = r4;
  Register holder = r2;
  Register api_function_address = r1;

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
      __ ldr(holder, FieldMemOperand(receiver, HeapObject::kMapOffset));
      __ ldr(holder, FieldMemOperand(holder, Map::kPrototypeOffset));
      for (int i = 1; i < holder_depth; i++) {
        __ ldr(holder, FieldMemOperand(holder, HeapObject::kMapOffset));
        __ ldr(holder, FieldMemOperand(holder, Map::kPrototypeOffset));
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
    __ LoadRoot(data, Heap::kUndefinedValueRootIndex);
  } else {
    if (optimization.is_constant_call()) {
      __ ldr(data,
             FieldMemOperand(callee, JSFunction::kSharedFunctionInfoOffset));
      __ ldr(data,
             FieldMemOperand(data, SharedFunctionInfo::kFunctionDataOffset));
      __ ldr(data,
             FieldMemOperand(data, FunctionTemplateInfo::kCallCodeOffset));
    } else {
      __ ldr(data,
             FieldMemOperand(callee, FunctionTemplateInfo::kCallCodeOffset));
    }
    __ ldr(data, FieldMemOperand(data, CallHandlerInfo::kDataOffset));
  }

  if (api_call_info->fast_handler()->IsCode()) {
    // Just tail call into the fast handler if present.
    __ Jump(handle(Code::cast(api_call_info->fast_handler())),
            RelocInfo::CODE_TARGET);
    return;
  }

  // Put api_function_address in place.
  Address function_address = v8::ToCData<Address>(api_call_info->callback());
  ApiFunction fun(function_address);
  ExternalReference::Type type = ExternalReference::DIRECT_API_CALL;
  ExternalReference ref = ExternalReference(&fun, type, masm->isolate());
  __ mov(api_function_address, Operand(ref));

  // Jump to stub.
  CallApiCallbackStub stub(isolate, is_store, call_data_undefined,
                           !optimization.is_constant_call());
  __ TailCallStub(&stub);
}

#undef __
#define __ ACCESS_MASM(masm())


void NamedStoreHandlerCompiler::GenerateRestoreName(Label* label,
                                                    Handle<Name> name) {
  if (!label->is_unused()) {
    __ bind(label);
    __ mov(this->name(), Operand(name));
  }
}


void NamedStoreHandlerCompiler::GenerateRestoreName(Handle<Name> name) {
  __ mov(this->name(), Operand(name));
}


void NamedStoreHandlerCompiler::GenerateRestoreMap(Handle<Map> transition,
                                                   Register map_reg,
                                                   Register scratch,
                                                   Label* miss) {
  Handle<WeakCell> cell = Map::WeakCellForMap(transition);
  DCHECK(!map_reg.is(scratch));
  __ LoadWeakValue(map_reg, cell, miss);
  if (transition->CanBeDeprecated()) {
    __ ldr(scratch, FieldMemOperand(map_reg, Map::kBitField3Offset));
    __ tst(scratch, Operand(Map::Deprecated::kMask));
    __ b(ne, miss);
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
  __ ldr(scratch,
         FieldMemOperand(scratch, DescriptorArray::GetValueOffset(descriptor)));
  __ cmp(value_reg, scratch);
  __ b(ne, miss_label);
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
    __ ldr(map_reg, FieldMemOperand(value_reg, HeapObject::kMapOffset));
    __ CmpWeakValue(map_reg, Map::WeakCellForMap(field_type->AsClass()),
                    scratch);
    __ b(ne, miss_label);
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
    __ mov(scratch1, Operand(validity_cell));
    __ ldr(scratch1, FieldMemOperand(scratch1, Cell::kValueOffset));
    __ cmp(scratch1, Operand(Smi::FromInt(Map::kPrototypeChainValid)));
    __ b(ne, miss);
  }

  // The prototype chain of primitives (and their JSValue wrappers) depends
  // on the native context, which can't be guarded by validity cells.
  // |object_reg| holds the native context specific prototype in this case;
  // we need to check its map.
  if (check == CHECK_ALL_MAPS) {
    __ ldr(scratch1, FieldMemOperand(object_reg, HeapObject::kMapOffset));
    Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);
    __ CmpWeakValue(scratch1, cell, scratch2);
    __ b(ne, miss);
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
    __ CheckAccessGlobalProxy(reg, scratch2, miss);
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
    __ b(&success);
    __ bind(miss);
    if (IC::ICUseVector(kind())) {
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
    __ b(&success);
    GenerateRestoreName(miss, name);
    if (IC::ICUseVector(kind())) PopVectorAndSlot();
    TailCallBuiltin(masm(), MissBuiltin(kind()));
    __ bind(&success);
  }
}


void NamedLoadHandlerCompiler::GenerateLoadConstant(Handle<Object> value) {
  // Return the constant value.
  __ Move(r0, value);
  __ Ret();
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
    FrameAndConstantPoolScope frame_scope(masm(), StackFrame::INTERNAL);
    if (must_preserve_receiver_reg) {
      __ Push(receiver(), holder_reg, this->name());
    } else {
      __ Push(holder_reg, this->name());
    }
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
    __ LoadRoot(scratch1(), Heap::kNoInterceptorResultSentinelRootIndex);
    __ cmp(r0, scratch1());
    __ b(eq, &interceptor_failed);
    frame_scope.GenerateLeaveFrame();
    __ Ret();

    __ bind(&interceptor_failed);
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
  // Call the runtime system to load the interceptor.
  DCHECK(holder()->HasNamedInterceptor());
  DCHECK(!holder()->GetNamedInterceptor()->getter()->IsUndefined(isolate()));
  PushInterceptorArguments(masm(), receiver(), holder_reg, this->name(),
                           holder());

  __ TailCallRuntime(Runtime::kLoadPropertyWithInterceptor);
}

void NamedStoreHandlerCompiler::ZapStackArgumentsRegisterAliases() {
  STATIC_ASSERT(!StoreWithVectorDescriptor::kPassLastArgsOnStack);
}

Handle<Code> NamedStoreHandlerCompiler::CompileStoreCallback(
    Handle<JSObject> object, Handle<Name> name, Handle<AccessorInfo> callback,
    LanguageMode language_mode) {
  Register holder_reg = Frontend(name);

  __ push(receiver());  // receiver
  __ push(holder_reg);

  // If the callback cannot leak, then push the callback directly,
  // otherwise wrap it in a weak cell.
  if (callback->data()->IsUndefined(isolate()) || callback->data()->IsSmi()) {
    __ mov(ip, Operand(callback));
  } else {
    Handle<WeakCell> cell = isolate()->factory()->NewWeakCell(callback);
    __ mov(ip, Operand(cell));
  }
  __ push(ip);
  __ mov(ip, Operand(name));
  __ Push(ip, value());
  __ Push(Smi::FromInt(language_mode));

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
  if (IC::ICUseVector(kind())) {
    PushVectorAndSlot();
  }
  FrontendHeader(receiver(), name, &miss, DONT_RETURN_ANYTHING);

  // Get the value from the cell.
  Register result = StoreDescriptor::ValueRegister();
  Handle<WeakCell> weak_cell = factory()->NewWeakCell(cell);
  __ LoadWeakValue(result, weak_cell, &miss);
  __ ldr(result, FieldMemOperand(result, PropertyCell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (is_configurable) {
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmp(result, ip);
    __ b(eq, &miss);
  }

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->ic_named_load_global_stub(), 1, r1, r3);
  if (IC::ICUseVector(kind())) {
    DiscardVectorAndSlot();
  }
  __ Ret();

  FrontendFooter(name, &miss);

  // Return the generated code.
  return GetCode(kind(), name);
}


#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM
