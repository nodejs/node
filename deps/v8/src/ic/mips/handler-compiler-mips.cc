// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_MIPS

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
    __ pop(cp);
  }
  __ Ret();
}


void NamedStoreHandlerCompiler::GenerateStoreViaSetter(
    MacroAssembler* masm, Handle<Map> map, Register receiver, Register holder,
    int accessor_index, int expected_arguments, Register scratch) {
  // ----------- S t a t e -------------
  //  -- ra    : return address
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save context and value registers, so we can restore them later.
    __ Push(cp, value());

    if (accessor_index >= 0) {
      DCHECK(!holder.is(scratch));
      DCHECK(!receiver.is(scratch));
      DCHECK(!value().is(scratch));
      // Call the JavaScript setter with receiver and value on the stack.
      if (map->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ lw(scratch,
              FieldMemOperand(receiver, JSGlobalObject::kGlobalProxyOffset));
        receiver = scratch;
      }
      __ Push(receiver, value());
      __ LoadAccessor(a1, holder, accessor_index, ACCESSOR_SETTER);
      __ li(a0, Operand(1));
      __ Call(masm->isolate()->builtins()->CallFunction(
                  ConvertReceiverMode::kNotNullOrUndefined),
              RelocInfo::CODE_TARGET);
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetSetterStubDeoptPCOffset(masm->pc_offset());
    }

    // We have to return the passed value, not the return value of the setter.
    // Restore context register.
    __ Pop(cp, v0);
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
  __ Push(slot, vector);
}


void PropertyHandlerCompiler::PopVectorAndSlot(Register vector, Register slot) {
  MacroAssembler* masm = this->masm();
  __ Pop(slot, vector);
}


void PropertyHandlerCompiler::DiscardVectorAndSlot() {
  MacroAssembler* masm = this->masm();
  // Remove vector and slot.
  __ Addu(sp, sp, Operand(2 * kPointerSize));
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
  __ lw(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ lbu(scratch0, FieldMemOperand(map, Map::kBitFieldOffset));
  __ And(scratch0, scratch0, Operand(kInterceptorOrAccessCheckNeededMask));
  __ Branch(miss_label, ne, scratch0, Operand(zero_reg));

  // Check that receiver is a JSObject.
  __ lbu(scratch0, FieldMemOperand(map, Map::kInstanceTypeOffset));
  __ Branch(miss_label, lt, scratch0, Operand(FIRST_JS_RECEIVER_TYPE));

  // Load properties array.
  Register properties = scratch0;
  __ lw(properties, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  // Check that the properties array is a dictionary.
  __ lw(map, FieldMemOperand(properties, HeapObject::kMapOffset));
  Register tmp = properties;
  __ LoadRoot(tmp, Heap::kHashTableMapRootIndex);
  __ Branch(miss_label, ne, map, Operand(tmp));

  // Restore the temporarily used register.
  __ lw(properties, FieldMemOperand(receiver, JSObject::kPropertiesOffset));


  NameDictionaryLookupStub::GenerateNegativeLookup(
      masm, miss_label, &done, receiver, properties, name, scratch1);
  __ bind(&done);
  __ DecrementCounter(counters->negative_lookups_miss(), 1, scratch0, scratch1);
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
  __ lw(scratch, FieldMemOperand(scratch, PropertyCell::kValueOffset));
  __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
  __ Branch(miss, ne, scratch, Operand(at));
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
  Register callee = a0;
  Register data = t0;
  Register holder = a2;
  Register api_function_address = a1;

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
      __ lw(holder, FieldMemOperand(receiver, HeapObject::kMapOffset));
      __ lw(holder, FieldMemOperand(holder, Map::kPrototypeOffset));
      for (int i = 1; i < holder_depth; i++) {
        __ lw(holder, FieldMemOperand(holder, HeapObject::kMapOffset));
        __ lw(holder, FieldMemOperand(holder, Map::kPrototypeOffset));
      }
      break;
    case CallOptimization::kHolderNotFound:
      UNREACHABLE();
      break;
  }

  Isolate* isolate = masm->isolate();
  Handle<CallHandlerInfo> api_call_info = optimization.api_call_info();
  // Put call data in place.
  if (api_call_info->data()->IsUndefined(isolate)) {
    __ LoadRoot(data, Heap::kUndefinedValueRootIndex);
  } else {
    if (optimization.is_constant_call()) {
      __ lw(data,
            FieldMemOperand(callee, JSFunction::kSharedFunctionInfoOffset));
      __ lw(data,
            FieldMemOperand(data, SharedFunctionInfo::kFunctionDataOffset));
      __ lw(data, FieldMemOperand(data, FunctionTemplateInfo::kCallCodeOffset));
    } else {
      __ lw(data,
            FieldMemOperand(callee, FunctionTemplateInfo::kCallCodeOffset));
    }
    __ lw(data, FieldMemOperand(data, CallHandlerInfo::kDataOffset));
  }

  // Put api_function_address in place.
  Address function_address = v8::ToCData<Address>(api_call_info->callback());
  ApiFunction fun(function_address);
  ExternalReference::Type type = ExternalReference::DIRECT_API_CALL;
  ExternalReference ref = ExternalReference(&fun, type, masm->isolate());
  __ li(api_function_address, Operand(ref));

  // Jump to stub.
  CallApiCallbackStub stub(isolate, is_store, !optimization.is_constant_call());
  __ TailCallStub(&stub);
}

#undef __
#define __ ACCESS_MASM(masm())


void NamedStoreHandlerCompiler::GenerateRestoreName(Label* label,
                                                    Handle<Name> name) {
  if (!label->is_unused()) {
    __ bind(label);
    __ li(this->name(), Operand(name));
  }
}

void PropertyHandlerCompiler::GenerateAccessCheck(
    Handle<WeakCell> native_context_cell, Register scratch1, Register scratch2,
    Label* miss, bool compare_native_contexts_only) {
  Label done;
  // Load current native context.
  __ lw(scratch1, NativeContextMemOperand());
  // Load expected native context.
  __ LoadWeakValue(scratch2, native_context_cell, miss);

  if (!compare_native_contexts_only) {
    __ Branch(&done, eq, scratch1, Operand(scratch2));

    // Compare security tokens of current and expected native contexts.
    __ lw(scratch1, ContextMemOperand(scratch1, Context::SECURITY_TOKEN_INDEX));
    __ lw(scratch2, ContextMemOperand(scratch2, Context::SECURITY_TOKEN_INDEX));
  }
  __ Branch(miss, ne, scratch1, Operand(scratch2));

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
    __ li(scratch1, Operand(validity_cell));
    __ lw(scratch1, FieldMemOperand(scratch1, Cell::kValueOffset));
    __ Branch(miss, ne, scratch1,
              Operand(Smi::FromInt(Map::kPrototypeChainValid)));
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
    __ Branch(&success);
    __ bind(miss);
    DCHECK(kind() == Code::LOAD_IC);
    PopVectorAndSlot();
    TailCallBuiltin(masm(), MissBuiltin(kind()));
    __ bind(&success);
  }
}


void NamedStoreHandlerCompiler::FrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ Branch(&success);
    GenerateRestoreName(miss, name);
    PopVectorAndSlot();
    TailCallBuiltin(masm(), MissBuiltin(kind()));
    __ bind(&success);
  }
}

void NamedStoreHandlerCompiler::ZapStackArgumentsRegisterAliases() {
  STATIC_ASSERT(!StoreWithVectorDescriptor::kPassLastArgsOnStack);
}

Handle<Code> NamedStoreHandlerCompiler::CompileStoreCallback(
    Handle<JSObject> object, Handle<Name> name, Handle<AccessorInfo> callback,
    LanguageMode language_mode) {
  Register holder_reg = Frontend(name);

  __ Push(receiver(), holder_reg);  // Receiver.
  // If the callback cannot leak, then push the callback directly,
  // otherwise wrap it in a weak cell.
  if (callback->data()->IsUndefined(isolate()) || callback->data()->IsSmi()) {
    __ li(at, Operand(callback));
  } else {
    Handle<WeakCell> cell = isolate()->factory()->NewWeakCell(callback);
    __ li(at, Operand(cell));
  }
  __ push(at);
  __ li(at, Operand(name));
  __ Push(at, value());
  __ Push(Smi::FromInt(language_mode));

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

#endif  // V8_TARGET_ARCH_MIPS
