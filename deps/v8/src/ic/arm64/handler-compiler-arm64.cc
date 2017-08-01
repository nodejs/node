// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/ic/handler-compiler.h"

#include "src/api-arguments.h"
#include "src/arm64/assembler-arm64-inl.h"
#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/field-type.h"
#include "src/ic/call-optimization.h"
#include "src/ic/ic.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void PropertyHandlerCompiler::PushVectorAndSlot(Register vector,
                                                Register slot) {
  MacroAssembler* masm = this->masm();
  STATIC_ASSERT(LoadWithVectorDescriptor::kSlot <
                LoadWithVectorDescriptor::kVector);
  STATIC_ASSERT(StoreWithVectorDescriptor::kSlot <
                StoreWithVectorDescriptor::kVector);
  STATIC_ASSERT(StoreTransitionDescriptor::kSlot <
                StoreTransitionDescriptor::kVector);
  __ Push(slot);
  __ Push(vector);
}


void PropertyHandlerCompiler::PopVectorAndSlot(Register vector, Register slot) {
  MacroAssembler* masm = this->masm();
  __ Pop(vector);
  __ Pop(slot);
}


void PropertyHandlerCompiler::DiscardVectorAndSlot() {
  MacroAssembler* masm = this->masm();
  // Remove vector and slot.
  __ Drop(2);
}

void PropertyHandlerCompiler::GenerateDictionaryNegativeLookup(
    MacroAssembler* masm, Label* miss_label, Register receiver,
    Handle<Name> name, Register scratch0, Register scratch1) {
  DCHECK(!AreAliased(receiver, scratch0, scratch1));
  DCHECK(name->IsUniqueName());
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->negative_lookups(), 1, scratch0, scratch1);
  __ IncrementCounter(counters->negative_lookups_miss(), 1, scratch0, scratch1);

  Label done;

  const int kInterceptorOrAccessCheckNeededMask =
      (1 << Map::kHasNamedInterceptor) | (1 << Map::kIsAccessCheckNeeded);

  // Bail out if the receiver has a named interceptor or requires access checks.
  Register map = scratch1;
  __ Ldr(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Ldrb(scratch0, FieldMemOperand(map, Map::kBitFieldOffset));
  __ Tst(scratch0, kInterceptorOrAccessCheckNeededMask);
  __ B(ne, miss_label);

  // Check that receiver is a JSObject.
  __ Ldrb(scratch0, FieldMemOperand(map, Map::kInstanceTypeOffset));
  __ Cmp(scratch0, FIRST_JS_RECEIVER_TYPE);
  __ B(lt, miss_label);

  // Load properties array.
  Register properties = scratch0;
  __ Ldr(properties, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  // Check that the properties array is a dictionary.
  __ Ldr(map, FieldMemOperand(properties, HeapObject::kMapOffset));
  __ JumpIfNotRoot(map, Heap::kHashTableMapRootIndex, miss_label);

  NameDictionaryLookupStub::GenerateNegativeLookup(
      masm, miss_label, &done, receiver, properties, name, scratch1);
  __ Bind(&done);
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
  __ Ldr(scratch, FieldMemOperand(scratch, PropertyCell::kValueOffset));
  __ JumpIfNotRoot(scratch, Heap::kTheHoleValueRootIndex, miss);
}

// Generate call to api function.
void PropertyHandlerCompiler::GenerateApiAccessorCall(
    MacroAssembler* masm, const CallOptimization& optimization,
    Handle<Map> receiver_map, Register receiver, Register scratch,
    bool is_store, Register store_parameter, Register accessor_holder,
    int accessor_index) {
  DCHECK(!AreAliased(accessor_holder, scratch));
  DCHECK(!AreAliased(receiver, scratch));

  MacroAssembler::PushPopQueue queue(masm);
  queue.Queue(receiver);
  // Write the arguments to the stack frame.
  if (is_store) {
    DCHECK(!receiver.is(store_parameter));
    DCHECK(!scratch.is(store_parameter));
    queue.Queue(store_parameter);
  }
  queue.PushQueued();

  DCHECK(optimization.is_simple_api_call());

  // Abi for CallApiCallbackStub.
  Register callee = x0;
  Register data = x4;
  Register holder = x2;
  Register api_function_address = x1;

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
      __ Mov(holder, receiver);
      break;
    case CallOptimization::kHolderFound:
      __ Ldr(holder, FieldMemOperand(receiver, HeapObject::kMapOffset));
      __ Ldr(holder, FieldMemOperand(holder, Map::kPrototypeOffset));
      for (int i = 1; i < holder_depth; i++) {
        __ Ldr(holder, FieldMemOperand(holder, HeapObject::kMapOffset));
        __ Ldr(holder, FieldMemOperand(holder, Map::kPrototypeOffset));
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
      __ Ldr(data,
             FieldMemOperand(callee, JSFunction::kSharedFunctionInfoOffset));
      __ Ldr(data,
             FieldMemOperand(data, SharedFunctionInfo::kFunctionDataOffset));
      __ Ldr(data,
             FieldMemOperand(data, FunctionTemplateInfo::kCallCodeOffset));
    } else {
      __ Ldr(data,
             FieldMemOperand(callee, FunctionTemplateInfo::kCallCodeOffset));
    }
    __ Ldr(data, FieldMemOperand(data, CallHandlerInfo::kDataOffset));
  }

  // Put api_function_address in place.
  Address function_address = v8::ToCData<Address>(api_call_info->callback());
  ApiFunction fun(function_address);
  ExternalReference ref = ExternalReference(
      &fun, ExternalReference::DIRECT_API_CALL, masm->isolate());
  __ Mov(api_function_address, ref);

  // Jump to stub.
  CallApiCallbackStub stub(isolate, is_store, !optimization.is_constant_call());
  __ TailCallStub(&stub);
}


void NamedStoreHandlerCompiler::GenerateStoreViaSetter(
    MacroAssembler* masm, Handle<Map> map, Register receiver, Register holder,
    int accessor_index, int expected_arguments, Register scratch) {
  // ----------- S t a t e -------------
  //  -- lr    : return address
  // -----------------------------------
  Label miss;
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save context register
    __ Push(cp);
    // Save value register, so we can restore it later.
    __ Push(value());

    if (accessor_index >= 0) {
      DCHECK(!AreAliased(holder, scratch));
      DCHECK(!AreAliased(receiver, scratch));
      DCHECK(!AreAliased(value(), scratch));
      // Call the JavaScript setter with receiver and value on the stack.
      if (map->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ Ldr(scratch,
               FieldMemOperand(receiver, JSGlobalObject::kGlobalProxyOffset));
        receiver = scratch;
      }
      __ Push(receiver, value());
      __ LoadAccessor(x1, holder, accessor_index, ACCESSOR_SETTER);
      __ Mov(x0, 1);
      __ Call(masm->isolate()->builtins()->CallFunction(
                  ConvertReceiverMode::kNotNullOrUndefined),
              RelocInfo::CODE_TARGET);
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetSetterStubDeoptPCOffset(masm->pc_offset());
    }

    // We have to return the passed value, not the return value of the setter.
    __ Pop(x0);

    // Restore context register.
    __ Pop(cp);
  }
  __ Ret();
}

void NamedLoadHandlerCompiler::GenerateLoadViaGetterForDeopt(
    MacroAssembler* masm) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    // If we generate a global code snippet for deoptimization only, remember
    // the place to continue after deoptimization.
    masm->isolate()->heap()->SetGetterStubDeoptPCOffset(masm->pc_offset());
    // Restore context register.
    __ Pop(cp);
  }
  __ Ret();
}

#undef __
#define __ ACCESS_MASM(masm())


Register NamedStoreHandlerCompiler::value() {
  return StoreDescriptor::ValueRegister();
}


void NamedStoreHandlerCompiler::GenerateRestoreName(Label* label,
                                                    Handle<Name> name) {
  if (!label->is_unused()) {
    __ Bind(label);
    __ Mov(this->name(), Operand(name));
  }
}

void PropertyHandlerCompiler::GenerateAccessCheck(
    Handle<WeakCell> native_context_cell, Register scratch1, Register scratch2,
    Label* miss, bool compare_native_contexts_only) {
  Label done;
  // Load current native context.
  __ Ldr(scratch1, NativeContextMemOperand());
  // Load expected native context.
  __ LoadWeakValue(scratch2, native_context_cell, miss);
  __ Cmp(scratch1, scratch2);

  if (!compare_native_contexts_only) {
    __ B(eq, &done);

    // Compare security tokens of current and expected native contexts.
    __ Ldr(scratch1,
           ContextMemOperand(scratch1, Context::SECURITY_TOKEN_INDEX));
    __ Ldr(scratch2,
           ContextMemOperand(scratch2, Context::SECURITY_TOKEN_INDEX));
    __ Cmp(scratch1, scratch2);
  }
  __ B(ne, miss);

  __ bind(&done);
}

Register PropertyHandlerCompiler::CheckPrototypes(
    Register object_reg, Register holder_reg, Register scratch1,
    Register scratch2, Handle<Name> name, Label* miss) {
  Handle<Map> receiver_map = map();

  // object_reg and holder_reg registers can alias.
  DCHECK(!AreAliased(object_reg, scratch1, scratch2));
  DCHECK(!AreAliased(holder_reg, scratch1, scratch2));

  Handle<Cell> validity_cell =
      Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate());
  if (!validity_cell.is_null()) {
    DCHECK_EQ(Smi::FromInt(Map::kPrototypeChainValid), validity_cell->value());
    __ Mov(scratch1, Operand(validity_cell));
    __ Ldr(scratch1, FieldMemOperand(scratch1, Cell::kValueOffset));
    // Compare scratch1 against Map::kPrototypeChainValid.
    static_assert(Map::kPrototypeChainValid == 0,
                  "Map::kPrototypeChainValid has unexpected value");
    __ Cbnz(scratch1, miss);
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
      DCHECK(current.is_null() || (current->property_dictionary()->FindEntry(
                                       name) == NameDictionary::kNotFound));

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
    __ B(&success);

    __ Bind(miss);
    DCHECK(kind() == Code::LOAD_IC);
    PopVectorAndSlot();
    TailCallBuiltin(masm(), MissBuiltin(kind()));

    __ Bind(&success);
  }
}


void NamedStoreHandlerCompiler::FrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ B(&success);

    GenerateRestoreName(miss, name);
    PopVectorAndSlot();
    TailCallBuiltin(masm(), MissBuiltin(kind()));

    __ Bind(&success);
  }
}

void NamedStoreHandlerCompiler::ZapStackArgumentsRegisterAliases() {
  STATIC_ASSERT(!StoreWithVectorDescriptor::kPassLastArgsOnStack);
}

Handle<Code> NamedStoreHandlerCompiler::CompileStoreCallback(
    Handle<JSObject> object, Handle<Name> name, Handle<AccessorInfo> callback,
    LanguageMode language_mode) {
  ASM_LOCATION("NamedStoreHandlerCompiler::CompileStoreCallback");
  Register holder_reg = Frontend(name);

  // Stub never generated for non-global objects that require access checks.
  DCHECK(holder()->IsJSGlobalProxy() || !holder()->IsAccessCheckNeeded());

  // receiver() and holder_reg can alias.
  DCHECK(!AreAliased(receiver(), scratch1(), scratch2(), value()));
  DCHECK(!AreAliased(holder_reg, scratch1(), scratch2(), value()));
  // If the callback cannot leak, then push the callback directly,
  // otherwise wrap it in a weak cell.
  if (callback->data()->IsUndefined(isolate()) || callback->data()->IsSmi()) {
    __ Mov(scratch1(), Operand(callback));
  } else {
    Handle<WeakCell> cell = isolate()->factory()->NewWeakCell(callback);
    __ Mov(scratch1(), Operand(cell));
  }
  __ Mov(scratch2(), Operand(name));
  __ Push(receiver(), holder_reg, scratch1(), scratch2(), value());
  __ Push(Smi::FromInt(language_mode));

  // Do tail-call to the runtime system.
  __ TailCallRuntime(Runtime::kStoreCallbackProperty);

  // Return the generated code.
  return GetCode(kind(), name);
}


#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32
