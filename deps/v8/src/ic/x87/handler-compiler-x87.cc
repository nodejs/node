// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X87

#include "src/ic/handler-compiler.h"

#include "src/api-arguments.h"
#include "src/field-type.h"
#include "src/ic/call-optimization.h"
#include "src/ic/ic.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void NamedLoadHandlerCompiler::GenerateLoadViaGetterForDeopt(
    MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // If we generate a global code snippet for deoptimization only, remember
    // the place to continue after deoptimization.
    masm->isolate()->heap()->SetGetterStubDeoptPCOffset(masm->pc_offset());
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

#undef __
#define __ ACCESS_MASM(masm())


void NamedStoreHandlerCompiler::GenerateRestoreName(Label* label,
                                                    Handle<Name> name) {
  if (!label->is_unused()) {
    __ bind(label);
    __ mov(this->name(), Immediate(name));
  }
}

void PropertyHandlerCompiler::GenerateAccessCheck(
    Handle<WeakCell> native_context_cell, Register scratch1, Register scratch2,
    Label* miss, bool compare_native_contexts_only) {
  Label done;
  // Load current native context.
  __ mov(scratch1, NativeContextOperand());
  // Load expected native context.
  __ LoadWeakValue(scratch2, native_context_cell, miss);
  __ cmp(scratch1, scratch2);

  if (!compare_native_contexts_only) {
    __ j(equal, &done);

    // Compare security tokens of current and expected native contexts.
    __ mov(scratch1, ContextOperand(scratch1, Context::SECURITY_TOKEN_INDEX));
    __ mov(scratch2, ContextOperand(scratch2, Context::SECURITY_TOKEN_INDEX));
    __ cmp(scratch1, scratch2);
  }
  __ j(not_equal, miss);

  __ bind(&done);
}

Register PropertyHandlerCompiler::CheckPrototypes(
    Register object_reg, Register holder_reg, Register scratch1,
    Register scratch2, Handle<Name> name, Label* miss) {
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

  // Keep track of the current object in register reg.
  Register reg = object_reg;
  int depth = 0;

  Handle<JSObject> current = Handle<JSObject>::null();
  if (receiver_map->IsJSGlobalObjectMap()) {
    current = isolate()->global_object();
  }

  Handle<Map> current_map(receiver_map->GetPrototypeChainRootMap(isolate()),
                          isolate());
  Handle<Map> holder_map(holder()->map());
  // Traverse the prototype chain and check the maps in the prototype chain for
  // fast and global objects or do negative lookup for normal objects.
  while (!current_map.is_identical_to(holder_map)) {
    ++depth;

    if (current_map->IsJSGlobalObjectMap()) {
      GenerateCheckPropertyCell(masm(), Handle<JSGlobalObject>::cast(current),
                                name, scratch2, miss);
    } else if (current_map->is_dictionary_map()) {
      DCHECK(!current_map->IsJSGlobalProxyMap());  // Proxy maps are fast.
      DCHECK(name->IsUniqueName());
      DCHECK(current.is_null() ||
             current->property_dictionary()->FindEntry(name) ==
                 NameDictionary::kNotFound);

      if (depth > 1) {
        Handle<WeakCell> weak_cell =
            Map::GetOrCreatePrototypeWeakCell(current, isolate());
        __ LoadWeakValue(reg, weak_cell, miss);
      }
      GenerateDictionaryNegativeLookup(masm(), miss, reg, name, scratch1,
                                       scratch2);
    }

    reg = holder_reg;  // From now on the object will be in holder_reg.
    // Go to the next object in the prototype chain.
    current = handle(JSObject::cast(current_map->prototype()));
    current_map = handle(current->map());
  }

  DCHECK(!current_map->IsJSGlobalProxyMap());

  // Log the check depth.
  LOG(isolate(), IntEvent("check-maps-depth", depth + 1));

  if (depth != 0) {
    Handle<WeakCell> weak_cell =
        Map::GetOrCreatePrototypeWeakCell(current, isolate());
    __ LoadWeakValue(reg, weak_cell, miss);
  }

  // Return the register containing the holder.
  return reg;
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


#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X87
