// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/ic/call-optimization.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void PropertyHandlerCompiler::PushVectorAndSlot(Register vector,
                                                Register slot) {
  MacroAssembler* masm = this->masm();
  __ Push(vector);
  __ Push(slot);
}


void PropertyHandlerCompiler::PopVectorAndSlot(Register vector, Register slot) {
  MacroAssembler* masm = this->masm();
  __ Pop(slot);
  __ Pop(vector);
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


void NamedLoadHandlerCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm, int index, Register result, Label* miss) {
  __ LoadNativeContextSlot(index, result);
  // Load its initial map. The global functions all have initial maps.
  __ Ldr(result,
         FieldMemOperand(result, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the prototype from the initial map.
  __ Ldr(result, FieldMemOperand(result, Map::kPrototypeOffset));
}


void NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(
    MacroAssembler* masm, Register receiver, Register scratch1,
    Register scratch2, Label* miss_label) {
  __ TryGetFunctionPrototype(receiver, scratch1, scratch2, miss_label);
  // TryGetFunctionPrototype can't put the result directly in x0 because the
  // 3 inputs registers can't alias and we call this function from
  // LoadIC::GenerateFunctionPrototype, where receiver is x0. So we explicitly
  // move the result in x0.
  __ Mov(x0, scratch1);
  __ Ret();
}


// Generate code to check that a global property cell is empty. Create
// the property cell at compilation time if no cell exists for the
// property.
void PropertyHandlerCompiler::GenerateCheckPropertyCell(
    MacroAssembler* masm, Handle<JSGlobalObject> global, Handle<Name> name,
    Register scratch, Label* miss) {
  Handle<PropertyCell> cell = JSGlobalObject::EnsurePropertyCell(global, name);
  DCHECK(cell->value()->IsTheHole());
  Handle<WeakCell> weak_cell = masm->isolate()->factory()->NewWeakCell(cell);
  __ LoadWeakValue(scratch, weak_cell, miss);
  __ Ldr(scratch, FieldMemOperand(scratch, PropertyCell::kValueOffset));
  __ JumpIfNotRoot(scratch, Heap::kTheHoleValueRootIndex, miss);
}


static void PushInterceptorArguments(MacroAssembler* masm, Register receiver,
                                     Register holder, Register name,
                                     Handle<JSObject> holder_obj) {
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsNameIndex == 0);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsThisIndex == 1);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsHolderIndex == 2);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsLength == 3);

  __ Push(name, receiver, holder);
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

  // Abi for CallApiFunctionStub.
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
  bool call_data_undefined = false;
  // Put call data in place.
  if (api_call_info->data()->IsUndefined()) {
    call_data_undefined = true;
    __ LoadRoot(data, Heap::kUndefinedValueRootIndex);
  } else {
    __ Ldr(data,
           FieldMemOperand(callee, JSFunction::kSharedFunctionInfoOffset));
    __ Ldr(data,
           FieldMemOperand(data, SharedFunctionInfo::kFunctionDataOffset));
    __ Ldr(data, FieldMemOperand(data, FunctionTemplateInfo::kCallCodeOffset));
    __ Ldr(data, FieldMemOperand(data, CallHandlerInfo::kDataOffset));
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
  ExternalReference ref = ExternalReference(
      &fun, ExternalReference::DIRECT_API_CALL, masm->isolate());
  __ Mov(api_function_address, ref);

  // Jump to stub.
  CallApiAccessorStub stub(isolate, is_store, call_data_undefined);
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
      ParameterCount actual(1);
      ParameterCount expected(expected_arguments);
      __ LoadAccessor(x1, holder, accessor_index, ACCESSOR_SETTER);
      __ InvokeFunction(x1, expected, actual, CALL_FUNCTION,
                        CheckDebugStepCallWrapper());
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetSetterStubDeoptPCOffset(masm->pc_offset());
    }

    // We have to return the passed value, not the return value of the setter.
    __ Pop(x0);

    // Restore context register.
    __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  }
  __ Ret();
}


void NamedLoadHandlerCompiler::GenerateLoadViaGetter(
    MacroAssembler* masm, Handle<Map> map, Register receiver, Register holder,
    int accessor_index, int expected_arguments, Register scratch) {
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    if (accessor_index >= 0) {
      DCHECK(!AreAliased(holder, scratch));
      DCHECK(!AreAliased(receiver, scratch));
      // Call the JavaScript getter with the receiver on the stack.
      if (map->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ Ldr(scratch,
               FieldMemOperand(receiver, JSGlobalObject::kGlobalProxyOffset));
        receiver = scratch;
      }
      __ Push(receiver);
      ParameterCount actual(0);
      ParameterCount expected(expected_arguments);
      __ LoadAccessor(x1, holder, accessor_index, ACCESSOR_GETTER);
      __ InvokeFunction(x1, expected, actual, CALL_FUNCTION,
                        CheckDebugStepCallWrapper());
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetGetterStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context register.
    __ Ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  }
  __ Ret();
}


static void StoreIC_PushArgs(MacroAssembler* masm) {
  __ Push(StoreDescriptor::ReceiverRegister(), StoreDescriptor::NameRegister(),
          StoreDescriptor::ValueRegister(),
          VectorStoreICDescriptor::SlotRegister(),
          VectorStoreICDescriptor::VectorRegister());
}


void NamedStoreHandlerCompiler::GenerateSlow(MacroAssembler* masm) {
  StoreIC_PushArgs(masm);

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  __ TailCallRuntime(Runtime::kStoreIC_Slow);
}


void ElementHandlerCompiler::GenerateStoreSlow(MacroAssembler* masm) {
  ASM_LOCATION("ElementHandlerCompiler::GenerateStoreSlow");
  StoreIC_PushArgs(masm);

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  __ TailCallRuntime(Runtime::kKeyedStoreIC_Slow);
}


#undef __
#define __ ACCESS_MASM(masm())


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
  __ Ldr(result, FieldMemOperand(result, PropertyCell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (is_configurable) {
    __ JumpIfRoot(result, Heap::kTheHoleValueRootIndex, &miss);
  }

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->named_load_global_stub(), 1, x1, x3);
  if (IC::ICUseVector(kind())) {
    DiscardVectorAndSlot();
  }
  __ Ret();

  FrontendFooter(name, &miss);

  // Return the generated code.
  return GetCode(kind(), Code::NORMAL, name);
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreInterceptor(
    Handle<Name> name) {
  Label miss;

  ASM_LOCATION("NamedStoreHandlerCompiler::CompileStoreInterceptor");

  __ Push(receiver(), this->name(), value());

  // Do tail-call to the runtime system.
  __ TailCallRuntime(Runtime::kStorePropertyWithInterceptor);

  // Return the generated code.
  return GetCode(kind(), Code::FAST, name);
}


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


void NamedStoreHandlerCompiler::GenerateRestoreName(Handle<Name> name) {
  __ Mov(this->name(), Operand(name));
}


void NamedStoreHandlerCompiler::RearrangeVectorAndSlot(
    Register current_map, Register destination_map) {
  DCHECK(false);  // Not implemented.
}


void NamedStoreHandlerCompiler::GenerateRestoreMap(Handle<Map> transition,
                                                   Register map_reg,
                                                   Register scratch,
                                                   Label* miss) {
  Handle<WeakCell> cell = Map::WeakCellForMap(transition);
  DCHECK(!map_reg.is(scratch));
  __ LoadWeakValue(map_reg, cell, miss);
  if (transition->CanBeDeprecated()) {
    __ Ldrsw(scratch, FieldMemOperand(map_reg, Map::kBitField3Offset));
    __ TestAndBranchIfAnySet(scratch, Map::Deprecated::kMask, miss);
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
  __ Ldr(scratch,
         FieldMemOperand(scratch, DescriptorArray::GetValueOffset(descriptor)));
  __ Cmp(value_reg, scratch);
  __ B(ne, miss_label);
}


void NamedStoreHandlerCompiler::GenerateFieldTypeChecks(HeapType* field_type,
                                                        Register value_reg,
                                                        Label* miss_label) {
  Register map_reg = scratch1();
  Register scratch = scratch2();
  DCHECK(!value_reg.is(map_reg));
  DCHECK(!value_reg.is(scratch));
  __ JumpIfSmi(value_reg, miss_label);
  HeapType::Iterator<Map> it = field_type->Classes();
  if (!it.Done()) {
    __ Ldr(map_reg, FieldMemOperand(value_reg, HeapObject::kMapOffset));
    Label do_store;
    while (true) {
      __ CmpWeakValue(map_reg, Map::WeakCellForMap(it.Current()), scratch);
      it.Advance();
      if (it.Done()) {
        __ B(ne, miss_label);
        break;
      }
      __ B(eq, &do_store);
    }
    __ Bind(&do_store);
  }
}


Register PropertyHandlerCompiler::CheckPrototypes(
    Register object_reg, Register holder_reg, Register scratch1,
    Register scratch2, Handle<Name> name, Label* miss, PrototypeCheckType check,
    ReturnHolder return_what) {
  Handle<Map> receiver_map = map();

  // object_reg and holder_reg registers can alias.
  DCHECK(!AreAliased(object_reg, scratch1, scratch2));
  DCHECK(!AreAliased(holder_reg, scratch1, scratch2));

  if (FLAG_eliminate_prototype_chain_checks) {
    Handle<Cell> validity_cell =
        Map::GetOrCreatePrototypeChainValidityCell(receiver_map, isolate());
    if (!validity_cell.is_null()) {
      DCHECK_EQ(Smi::FromInt(Map::kPrototypeChainValid),
                validity_cell->value());
      __ Mov(scratch1, Operand(validity_cell));
      __ Ldr(scratch1, FieldMemOperand(scratch1, Cell::kValueOffset));
      __ Cmp(scratch1, Operand(Smi::FromInt(Map::kPrototypeChainValid)));
      __ B(ne, miss);
    }

    // The prototype chain of primitives (and their JSValue wrappers) depends
    // on the native context, which can't be guarded by validity cells.
    // |object_reg| holds the native context specific prototype in this case;
    // we need to check its map.
    if (check == CHECK_ALL_MAPS) {
      __ Ldr(scratch1, FieldMemOperand(object_reg, HeapObject::kMapOffset));
      Handle<WeakCell> cell = Map::WeakCellForMap(receiver_map);
      __ CmpWeakValue(scratch1, cell, scratch2);
      __ B(ne, miss);
    }
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
    UseScratchRegisterScope temps(masm());
    __ CheckAccessGlobalProxy(reg, scratch2, temps.AcquireX(), miss);
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
    if (current_map->is_dictionary_map() &&
        !current_map->IsJSGlobalObjectMap()) {
      DCHECK(!current_map->IsJSGlobalProxyMap());  // Proxy maps are fast.
      if (!name->IsUniqueName()) {
        DCHECK(name->IsString());
        name = factory()->InternalizeString(Handle<String>::cast(name));
      }
      DCHECK(current.is_null() || (current->property_dictionary()->FindEntry(
                                       name) == NameDictionary::kNotFound));

      if (FLAG_eliminate_prototype_chain_checks && depth > 1) {
        // TODO(jkummerow): Cache and re-use weak cell.
        __ LoadWeakValue(reg, isolate()->factory()->NewWeakCell(current), miss);
      }
      GenerateDictionaryNegativeLookup(masm(), miss, reg, name, scratch1,
                                       scratch2);

      if (!FLAG_eliminate_prototype_chain_checks) {
        __ Ldr(scratch1, FieldMemOperand(reg, HeapObject::kMapOffset));
        __ Ldr(holder_reg, FieldMemOperand(scratch1, Map::kPrototypeOffset));
      }
    } else {
      Register map_reg = scratch1;
      if (!FLAG_eliminate_prototype_chain_checks) {
        __ Ldr(map_reg, FieldMemOperand(reg, HeapObject::kMapOffset));
      }
      if (current_map->IsJSGlobalObjectMap()) {
        GenerateCheckPropertyCell(masm(), Handle<JSGlobalObject>::cast(current),
                                  name, scratch2, miss);
      } else if (!FLAG_eliminate_prototype_chain_checks &&
                 (depth != 1 || check == CHECK_ALL_MAPS)) {
        Handle<WeakCell> cell = Map::WeakCellForMap(current_map);
        __ CmpWeakValue(map_reg, cell, scratch2);
        __ B(ne, miss);
      }
      if (!FLAG_eliminate_prototype_chain_checks) {
        __ Ldr(holder_reg, FieldMemOperand(map_reg, Map::kPrototypeOffset));
      }
    }

    reg = holder_reg;  // From now on the object will be in holder_reg.
    // Go to the next object in the prototype chain.
    current = prototype;
    current_map = handle(current->map());
  }

  DCHECK(!current_map->IsJSGlobalProxyMap());

  // Log the check depth.
  LOG(isolate(), IntEvent("check-maps-depth", depth + 1));

  if (!FLAG_eliminate_prototype_chain_checks &&
      (depth != 0 || check == CHECK_ALL_MAPS)) {
    // Check the holder map.
    __ Ldr(scratch1, FieldMemOperand(reg, HeapObject::kMapOffset));
    Handle<WeakCell> cell = Map::WeakCellForMap(current_map);
    __ CmpWeakValue(scratch1, cell, scratch2);
    __ B(ne, miss);
  }

  bool return_holder = return_what == RETURN_HOLDER;
  if (FLAG_eliminate_prototype_chain_checks && return_holder && depth != 0) {
    __ LoadWeakValue(reg, isolate()->factory()->NewWeakCell(current), miss);
  }

  // Return the register containing the holder.
  return return_holder ? reg : no_reg;
}


void NamedLoadHandlerCompiler::FrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ B(&success);

    __ Bind(miss);
    if (IC::ICUseVector(kind())) {
      DCHECK(kind() == Code::LOAD_IC);
      PopVectorAndSlot();
    }
    TailCallBuiltin(masm(), MissBuiltin(kind()));

    __ Bind(&success);
  }
}


void NamedStoreHandlerCompiler::FrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ B(&success);

    GenerateRestoreName(miss, name);
    if (IC::ICUseVector(kind())) PopVectorAndSlot();
    TailCallBuiltin(masm(), MissBuiltin(kind()));

    __ Bind(&success);
  }
}


void NamedLoadHandlerCompiler::GenerateLoadConstant(Handle<Object> value) {
  // Return the constant value.
  __ LoadObject(x0, value);
  __ Ret();
}


void NamedLoadHandlerCompiler::GenerateLoadCallback(
    Register reg, Handle<ExecutableAccessorInfo> callback) {
  DCHECK(!AreAliased(scratch2(), scratch3(), scratch4(), reg));

  // Build ExecutableAccessorInfo::args_ list on the stack and push property
  // name below the exit frame to make GC aware of them and store pointers to
  // them.
  STATIC_ASSERT(PropertyCallbackArguments::kHolderIndex == 0);
  STATIC_ASSERT(PropertyCallbackArguments::kIsolateIndex == 1);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueOffset == 3);
  STATIC_ASSERT(PropertyCallbackArguments::kDataIndex == 4);
  STATIC_ASSERT(PropertyCallbackArguments::kThisIndex == 5);
  STATIC_ASSERT(PropertyCallbackArguments::kArgsLength == 6);

  __ Push(receiver());

  Handle<Object> data(callback->data(), isolate());
  if (data->IsUndefined() || data->IsSmi()) {
    __ Mov(scratch3(), Operand(data));
  } else {
    Handle<WeakCell> cell =
        isolate()->factory()->NewWeakCell(Handle<HeapObject>::cast(data));
    // The callback is alive if this instruction is executed,
    // so the weak cell is not cleared and points to data.
    __ GetWeakValue(scratch3(), cell);
  }
  __ LoadRoot(scratch4(), Heap::kUndefinedValueRootIndex);
  __ Mov(scratch2(), Operand(ExternalReference::isolate_address(isolate())));
  __ Push(scratch3(), scratch4(), scratch4(), scratch2(), reg, name());

  Register args_addr = scratch2();
  __ Add(args_addr, __ StackPointer(), kPointerSize);

  // Stack at this point:
  //              sp[40] callback data
  //              sp[32] undefined
  //              sp[24] undefined
  //              sp[16] isolate
  // args_addr -> sp[8]  reg
  //              sp[0]  name

  // Abi for CallApiGetter.
  Register getter_address_reg = x2;

  // Set up the call.
  Address getter_address = v8::ToCData<Address>(callback->getter());
  ApiFunction fun(getter_address);
  ExternalReference::Type type = ExternalReference::DIRECT_GETTER_CALL;
  ExternalReference ref = ExternalReference(&fun, type, isolate());
  __ Mov(getter_address_reg, ref);

  CallApiGetterStub stub(isolate());
  __ TailCallStub(&stub);
}


void NamedLoadHandlerCompiler::GenerateLoadInterceptorWithFollowup(
    LookupIterator* it, Register holder_reg) {
  DCHECK(!AreAliased(receiver(), this->name(), scratch1(), scratch2(),
                     scratch3()));
  DCHECK(holder()->HasNamedInterceptor());
  DCHECK(!holder()->GetNamedInterceptor()->getter()->IsUndefined());

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
    __ JumpIfRoot(x0, Heap::kNoInterceptorResultSentinelRootIndex,
                  &interceptor_failed);
    frame_scope.GenerateLeaveFrame();
    __ Ret();

    __ Bind(&interceptor_failed);
    InterceptorVectorSlotPop(holder_reg);
    if (must_preserve_receiver_reg) {
      __ Pop(this->name(), holder_reg, receiver());
    } else {
      __ Pop(this->name(), holder_reg);
    }
    // Leave the internal frame.
  }

  GenerateLoadPostInterceptor(it, holder_reg);
}


void NamedLoadHandlerCompiler::GenerateLoadInterceptor(Register holder_reg) {
  // Call the runtime system to load the interceptor.
  DCHECK(holder()->HasNamedInterceptor());
  DCHECK(!holder()->GetNamedInterceptor()->getter()->IsUndefined());
  PushInterceptorArguments(masm(), receiver(), holder_reg, this->name(),
                           holder());

  __ TailCallRuntime(Runtime::kLoadPropertyWithInterceptor);
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreCallback(
    Handle<JSObject> object, Handle<Name> name,
    Handle<ExecutableAccessorInfo> callback) {
  ASM_LOCATION("NamedStoreHandlerCompiler::CompileStoreCallback");
  Register holder_reg = Frontend(name);

  // Stub never generated for non-global objects that require access checks.
  DCHECK(holder()->IsJSGlobalProxy() || !holder()->IsAccessCheckNeeded());

  // receiver() and holder_reg can alias.
  DCHECK(!AreAliased(receiver(), scratch1(), scratch2(), value()));
  DCHECK(!AreAliased(holder_reg, scratch1(), scratch2(), value()));
  // If the callback cannot leak, then push the callback directly,
  // otherwise wrap it in a weak cell.
  if (callback->data()->IsUndefined() || callback->data()->IsSmi()) {
    __ Mov(scratch1(), Operand(callback));
  } else {
    Handle<WeakCell> cell = isolate()->factory()->NewWeakCell(callback);
    __ Mov(scratch1(), Operand(cell));
  }
  __ Mov(scratch2(), Operand(name));
  __ Push(receiver(), holder_reg, scratch1(), scratch2(), value());

  // Do tail-call to the runtime system.
  __ TailCallRuntime(Runtime::kStoreCallbackProperty);

  // Return the generated code.
  return GetCode(kind(), Code::FAST, name);
}


#undef __
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_IA32
