// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_X64

#include "src/ic/call-optimization.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void PropertyHandlerCompiler::GenerateDictionaryNegativeLookup(
    MacroAssembler* masm, Label* miss_label, Register receiver,
    Handle<Name> name, Register scratch0, Register scratch1) {
  DCHECK(name->IsUniqueName());
  DCHECK(!receiver.is(scratch0));
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->negative_lookups(), 1);
  __ IncrementCounter(counters->negative_lookups_miss(), 1);

  __ movp(scratch0, FieldOperand(receiver, HeapObject::kMapOffset));

  const int kInterceptorOrAccessCheckNeededMask =
      (1 << Map::kHasNamedInterceptor) | (1 << Map::kIsAccessCheckNeeded);

  // Bail out if the receiver has a named interceptor or requires access checks.
  __ testb(FieldOperand(scratch0, Map::kBitFieldOffset),
           Immediate(kInterceptorOrAccessCheckNeededMask));
  __ j(not_zero, miss_label);

  // Check that receiver is a JSObject.
  __ CmpInstanceType(scratch0, FIRST_SPEC_OBJECT_TYPE);
  __ j(below, miss_label);

  // Load properties array.
  Register properties = scratch0;
  __ movp(properties, FieldOperand(receiver, JSObject::kPropertiesOffset));

  // Check that the properties array is a dictionary.
  __ CompareRoot(FieldOperand(properties, HeapObject::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(not_equal, miss_label);

  Label done;
  NameDictionaryLookupStub::GenerateNegativeLookup(masm, miss_label, &done,
                                                   properties, name, scratch1);
  __ bind(&done);
  __ DecrementCounter(counters->negative_lookups_miss(), 1);
}


void NamedLoadHandlerCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm, int index, Register prototype, Label* miss) {
  Isolate* isolate = masm->isolate();
  // Get the global function with the given index.
  Handle<JSFunction> function(
      JSFunction::cast(isolate->native_context()->get(index)));

  // Check we're still in the same context.
  Register scratch = prototype;
  const int offset = Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX);
  __ movp(scratch, Operand(rsi, offset));
  __ movp(scratch, FieldOperand(scratch, GlobalObject::kNativeContextOffset));
  __ Cmp(Operand(scratch, Context::SlotOffset(index)), function);
  __ j(not_equal, miss);

  // Load its initial map. The global functions all have initial maps.
  __ Move(prototype, Handle<Map>(function->initial_map()));
  // Load the prototype from the initial map.
  __ movp(prototype, FieldOperand(prototype, Map::kPrototypeOffset));
}


void NamedLoadHandlerCompiler::GenerateLoadFunctionPrototype(
    MacroAssembler* masm, Register receiver, Register result, Register scratch,
    Label* miss_label) {
  __ TryGetFunctionPrototype(receiver, result, miss_label);
  if (!result.is(rax)) __ movp(rax, result);
  __ ret(0);
}


static void PushInterceptorArguments(MacroAssembler* masm, Register receiver,
                                     Register holder, Register name,
                                     Handle<JSObject> holder_obj) {
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsNameIndex == 0);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsInfoIndex == 1);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsThisIndex == 2);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsHolderIndex == 3);
  STATIC_ASSERT(NamedLoadHandlerCompiler::kInterceptorArgsLength == 4);
  __ Push(name);
  Handle<InterceptorInfo> interceptor(holder_obj->GetNamedInterceptor());
  DCHECK(!masm->isolate()->heap()->InNewSpace(*interceptor));
  __ Move(kScratchRegister, interceptor);
  __ Push(kScratchRegister);
  __ Push(receiver);
  __ Push(holder);
}


static void CompileCallLoadPropertyWithInterceptor(
    MacroAssembler* masm, Register receiver, Register holder, Register name,
    Handle<JSObject> holder_obj, IC::UtilityId id) {
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);
  __ CallExternalReference(ExternalReference(IC_Utility(id), masm->isolate()),
                           NamedLoadHandlerCompiler::kInterceptorArgsLength);
}


// Generate call to api function.
void PropertyHandlerCompiler::GenerateFastApiCall(
    MacroAssembler* masm, const CallOptimization& optimization,
    Handle<Map> receiver_map, Register receiver, Register scratch_in,
    bool is_store, int argc, Register* values) {
  DCHECK(optimization.is_simple_api_call());

  __ PopReturnAddressTo(scratch_in);
  // receiver
  __ Push(receiver);
  // Write the arguments to stack frame.
  for (int i = 0; i < argc; i++) {
    Register arg = values[argc - 1 - i];
    DCHECK(!receiver.is(arg));
    DCHECK(!scratch_in.is(arg));
    __ Push(arg);
  }
  __ PushReturnAddressFrom(scratch_in);
  // Stack now matches JSFunction abi.

  // Abi for CallApiFunctionStub.
  Register callee = rax;
  Register call_data = rbx;
  Register holder = rcx;
  Register api_function_address = rdx;
  Register scratch = rdi;  // scratch_in is no longer valid.

  // Put holder in place.
  CallOptimization::HolderLookup holder_lookup;
  Handle<JSObject> api_holder =
      optimization.LookupHolderOfExpectedType(receiver_map, &holder_lookup);
  switch (holder_lookup) {
    case CallOptimization::kHolderIsReceiver:
      __ Move(holder, receiver);
      break;
    case CallOptimization::kHolderFound:
      __ Move(holder, api_holder);
      break;
    case CallOptimization::kHolderNotFound:
      UNREACHABLE();
      break;
  }

  Isolate* isolate = masm->isolate();
  Handle<JSFunction> function = optimization.constant_function();
  Handle<CallHandlerInfo> api_call_info = optimization.api_call_info();
  Handle<Object> call_data_obj(api_call_info->data(), isolate);

  // Put callee in place.
  __ Move(callee, function);

  bool call_data_undefined = false;
  // Put call_data in place.
  if (isolate->heap()->InNewSpace(*call_data_obj)) {
    __ Move(scratch, api_call_info);
    __ movp(call_data, FieldOperand(scratch, CallHandlerInfo::kDataOffset));
  } else if (call_data_obj->IsUndefined()) {
    call_data_undefined = true;
    __ LoadRoot(call_data, Heap::kUndefinedValueRootIndex);
  } else {
    __ Move(call_data, call_data_obj);
  }

  // Put api_function_address in place.
  Address function_address = v8::ToCData<Address>(api_call_info->callback());
  __ Move(api_function_address, function_address,
          RelocInfo::EXTERNAL_REFERENCE);

  // Jump to stub.
  CallApiFunctionStub stub(isolate, is_store, call_data_undefined, argc);
  __ TailCallStub(&stub);
}


void PropertyHandlerCompiler::GenerateCheckPropertyCell(
    MacroAssembler* masm, Handle<JSGlobalObject> global, Handle<Name> name,
    Register scratch, Label* miss) {
  Handle<PropertyCell> cell = JSGlobalObject::EnsurePropertyCell(global, name);
  DCHECK(cell->value()->IsTheHole());
  __ Move(scratch, cell);
  __ Cmp(FieldOperand(scratch, Cell::kValueOffset),
         masm->isolate()->factory()->the_hole_value());
  __ j(not_equal, miss);
}


void NamedStoreHandlerCompiler::GenerateStoreViaSetter(
    MacroAssembler* masm, Handle<HeapType> type, Register receiver,
    Handle<JSFunction> setter) {
  // ----------- S t a t e -------------
  //  -- rsp[0] : return address
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save value register, so we can restore it later.
    __ Push(value());

    if (!setter.is_null()) {
      // Call the JavaScript setter with receiver and value on the stack.
      if (IC::TypeToMap(*type, masm->isolate())->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ movp(receiver,
                FieldOperand(receiver, JSGlobalObject::kGlobalProxyOffset));
      }
      __ Push(receiver);
      __ Push(value());
      ParameterCount actual(1);
      ParameterCount expected(setter);
      __ InvokeFunction(setter, expected, actual, CALL_FUNCTION,
                        NullCallWrapper());
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetSetterStubDeoptPCOffset(masm->pc_offset());
    }

    // We have to return the passed value, not the return value of the setter.
    __ Pop(rax);

    // Restore context register.
    __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  }
  __ ret(0);
}


void NamedLoadHandlerCompiler::GenerateLoadViaGetter(
    MacroAssembler* masm, Handle<HeapType> type, Register receiver,
    Handle<JSFunction> getter) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    if (!getter.is_null()) {
      // Call the JavaScript getter with the receiver on the stack.
      if (IC::TypeToMap(*type, masm->isolate())->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ movp(receiver,
                FieldOperand(receiver, JSGlobalObject::kGlobalProxyOffset));
      }
      __ Push(receiver);
      ParameterCount actual(0);
      ParameterCount expected(getter);
      __ InvokeFunction(getter, expected, actual, CALL_FUNCTION,
                        NullCallWrapper());
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetGetterStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context register.
    __ movp(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
  }
  __ ret(0);
}


static void StoreIC_PushArgs(MacroAssembler* masm) {
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  Register value = StoreDescriptor::ValueRegister();

  DCHECK(!rbx.is(receiver) && !rbx.is(name) && !rbx.is(value));

  __ PopReturnAddressTo(rbx);
  __ Push(receiver);
  __ Push(name);
  __ Push(value);
  __ PushReturnAddressFrom(rbx);
}


void NamedStoreHandlerCompiler::GenerateSlow(MacroAssembler* masm) {
  // Return address is on the stack.
  StoreIC_PushArgs(masm);

  // Do tail-call to runtime routine.
  ExternalReference ref(IC_Utility(IC::kStoreIC_Slow), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void ElementHandlerCompiler::GenerateStoreSlow(MacroAssembler* masm) {
  // Return address is on the stack.
  StoreIC_PushArgs(masm);

  // Do tail-call to runtime routine.
  ExternalReference ref(IC_Utility(IC::kKeyedStoreIC_Slow), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


#undef __
#define __ ACCESS_MASM((masm()))


void NamedStoreHandlerCompiler::GenerateRestoreName(Label* label,
                                                    Handle<Name> name) {
  if (!label->is_unused()) {
    __ bind(label);
    __ Move(this->name(), name);
  }
}


// Receiver_reg is preserved on jumps to miss_label, but may be destroyed if
// store is successful.
void NamedStoreHandlerCompiler::GenerateStoreTransition(
    Handle<Map> transition, Handle<Name> name, Register receiver_reg,
    Register storage_reg, Register value_reg, Register scratch1,
    Register scratch2, Register unused, Label* miss_label, Label* slow) {
  int descriptor = transition->LastAdded();
  DescriptorArray* descriptors = transition->instance_descriptors();
  PropertyDetails details = descriptors->GetDetails(descriptor);
  Representation representation = details.representation();
  DCHECK(!representation.IsNone());

  if (details.type() == CONSTANT) {
    Handle<Object> constant(descriptors->GetValue(descriptor), isolate());
    __ Cmp(value_reg, constant);
    __ j(not_equal, miss_label);
  } else if (representation.IsSmi()) {
    __ JumpIfNotSmi(value_reg, miss_label);
  } else if (representation.IsHeapObject()) {
    __ JumpIfSmi(value_reg, miss_label);
    HeapType* field_type = descriptors->GetFieldType(descriptor);
    HeapType::Iterator<Map> it = field_type->Classes();
    if (!it.Done()) {
      Label do_store;
      while (true) {
        __ CompareMap(value_reg, it.Current());
        it.Advance();
        if (it.Done()) {
          __ j(not_equal, miss_label);
          break;
        }
        __ j(equal, &do_store, Label::kNear);
      }
      __ bind(&do_store);
    }
  } else if (representation.IsDouble()) {
    Label do_store, heap_number;
    __ AllocateHeapNumber(storage_reg, scratch1, slow, MUTABLE);

    __ JumpIfNotSmi(value_reg, &heap_number);
    __ SmiToInteger32(scratch1, value_reg);
    __ Cvtlsi2sd(xmm0, scratch1);
    __ jmp(&do_store);

    __ bind(&heap_number);
    __ CheckMap(value_reg, isolate()->factory()->heap_number_map(), miss_label,
                DONT_DO_SMI_CHECK);
    __ movsd(xmm0, FieldOperand(value_reg, HeapNumber::kValueOffset));

    __ bind(&do_store);
    __ movsd(FieldOperand(storage_reg, HeapNumber::kValueOffset), xmm0);
  }

  // Stub never generated for objects that require access checks.
  DCHECK(!transition->is_access_check_needed());

  // Perform map transition for the receiver if necessary.
  if (details.type() == FIELD &&
      Map::cast(transition->GetBackPointer())->unused_property_fields() == 0) {
    // The properties must be extended before we can store the value.
    // We jump to a runtime call that extends the properties array.
    __ PopReturnAddressTo(scratch1);
    __ Push(receiver_reg);
    __ Push(transition);
    __ Push(value_reg);
    __ PushReturnAddressFrom(scratch1);
    __ TailCallExternalReference(
        ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage),
                          isolate()),
        3, 1);
    return;
  }

  // Update the map of the object.
  __ Move(scratch1, transition);
  __ movp(FieldOperand(receiver_reg, HeapObject::kMapOffset), scratch1);

  // Update the write barrier for the map field.
  __ RecordWriteField(receiver_reg, HeapObject::kMapOffset, scratch1, scratch2,
                      kDontSaveFPRegs, OMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

  if (details.type() == CONSTANT) {
    DCHECK(value_reg.is(rax));
    __ ret(0);
    return;
  }

  int index = transition->instance_descriptors()->GetFieldIndex(
      transition->LastAdded());

  // Adjust for the number of properties stored in the object. Even in the
  // face of a transition we can use the old map here because the size of the
  // object and the number of in-object properties is not going to change.
  index -= transition->inobject_properties();

  // TODO(verwaest): Share this code as a code stub.
  SmiCheck smi_check =
      representation.IsTagged() ? INLINE_SMI_CHECK : OMIT_SMI_CHECK;
  if (index < 0) {
    // Set the property straight into the object.
    int offset = transition->instance_size() + (index * kPointerSize);
    if (representation.IsDouble()) {
      __ movp(FieldOperand(receiver_reg, offset), storage_reg);
    } else {
      __ movp(FieldOperand(receiver_reg, offset), value_reg);
    }

    if (!representation.IsSmi()) {
      // Update the write barrier for the array address.
      if (!representation.IsDouble()) {
        __ movp(storage_reg, value_reg);
      }
      __ RecordWriteField(receiver_reg, offset, storage_reg, scratch1,
                          kDontSaveFPRegs, EMIT_REMEMBERED_SET, smi_check);
    }
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array (optimistically).
    __ movp(scratch1, FieldOperand(receiver_reg, JSObject::kPropertiesOffset));
    if (representation.IsDouble()) {
      __ movp(FieldOperand(scratch1, offset), storage_reg);
    } else {
      __ movp(FieldOperand(scratch1, offset), value_reg);
    }

    if (!representation.IsSmi()) {
      // Update the write barrier for the array address.
      if (!representation.IsDouble()) {
        __ movp(storage_reg, value_reg);
      }
      __ RecordWriteField(scratch1, offset, storage_reg, receiver_reg,
                          kDontSaveFPRegs, EMIT_REMEMBERED_SET, smi_check);
    }
  }

  // Return the value (register rax).
  DCHECK(value_reg.is(rax));
  __ ret(0);
}


void NamedStoreHandlerCompiler::GenerateStoreField(LookupIterator* lookup,
                                                   Register value_reg,
                                                   Label* miss_label) {
  DCHECK(lookup->representation().IsHeapObject());
  __ JumpIfSmi(value_reg, miss_label);
  HeapType::Iterator<Map> it = lookup->GetFieldType()->Classes();
  Label do_store;
  while (true) {
    __ CompareMap(value_reg, it.Current());
    it.Advance();
    if (it.Done()) {
      __ j(not_equal, miss_label);
      break;
    }
    __ j(equal, &do_store, Label::kNear);
  }
  __ bind(&do_store);

  StoreFieldStub stub(isolate(), lookup->GetFieldIndex(),
                      lookup->representation());
  GenerateTailCall(masm(), stub.GetCode());
}


Register PropertyHandlerCompiler::CheckPrototypes(
    Register object_reg, Register holder_reg, Register scratch1,
    Register scratch2, Handle<Name> name, Label* miss,
    PrototypeCheckType check) {
  Handle<Map> receiver_map(IC::TypeToMap(*type(), isolate()));

  // Make sure there's no overlap between holder and object registers.
  DCHECK(!scratch1.is(object_reg) && !scratch1.is(holder_reg));
  DCHECK(!scratch2.is(object_reg) && !scratch2.is(holder_reg) &&
         !scratch2.is(scratch1));

  // Keep track of the current object in register reg.  On the first
  // iteration, reg is an alias for object_reg, on later iterations,
  // it is an alias for holder_reg.
  Register reg = object_reg;
  int depth = 0;

  Handle<JSObject> current = Handle<JSObject>::null();
  if (type()->IsConstant()) {
    current = Handle<JSObject>::cast(type()->AsConstant()->Value());
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
      DCHECK(current.is_null() ||
             current->property_dictionary()->FindEntry(name) ==
                 NameDictionary::kNotFound);

      GenerateDictionaryNegativeLookup(masm(), miss, reg, name, scratch1,
                                       scratch2);

      __ movp(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // From now on the object will be in holder_reg.
      __ movp(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
    } else {
      bool in_new_space = heap()->InNewSpace(*prototype);
      // Two possible reasons for loading the prototype from the map:
      // (1) Can't store references to new space in code.
      // (2) Handler is shared for all receivers with the same prototype
      //     map (but not necessarily the same prototype instance).
      bool load_prototype_from_map = in_new_space || depth == 1;
      if (load_prototype_from_map) {
        // Save the map in scratch1 for later.
        __ movp(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      }
      if (depth != 1 || check == CHECK_ALL_MAPS) {
        __ CheckMap(reg, current_map, miss, DONT_DO_SMI_CHECK);
      }

      // Check access rights to the global object.  This has to happen after
      // the map check so that we know that the object is actually a global
      // object.
      // This allows us to install generated handlers for accesses to the
      // global proxy (as opposed to using slow ICs). See corresponding code
      // in LookupForRead().
      if (current_map->IsJSGlobalProxyMap()) {
        __ CheckAccessGlobalProxy(reg, scratch2, miss);
      } else if (current_map->IsJSGlobalObjectMap()) {
        GenerateCheckPropertyCell(masm(), Handle<JSGlobalObject>::cast(current),
                                  name, scratch2, miss);
      }
      reg = holder_reg;  // From now on the object will be in holder_reg.

      if (load_prototype_from_map) {
        __ movp(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
      } else {
        __ Move(reg, prototype);
      }
    }

    // Go to the next object in the prototype chain.
    current = prototype;
    current_map = handle(current->map());
  }

  // Log the check depth.
  LOG(isolate(), IntEvent("check-maps-depth", depth + 1));

  if (depth != 0 || check == CHECK_ALL_MAPS) {
    // Check the holder map.
    __ CheckMap(reg, current_map, miss, DONT_DO_SMI_CHECK);
  }

  // Perform security check for access to the global object.
  DCHECK(current_map->IsJSGlobalProxyMap() ||
         !current_map->is_access_check_needed());
  if (current_map->IsJSGlobalProxyMap()) {
    __ CheckAccessGlobalProxy(reg, scratch1, miss);
  }

  // Return the register containing the holder.
  return reg;
}


void NamedLoadHandlerCompiler::FrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ jmp(&success);
    __ bind(miss);
    TailCallBuiltin(masm(), MissBuiltin(kind()));
    __ bind(&success);
  }
}


void NamedStoreHandlerCompiler::FrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ jmp(&success);
    GenerateRestoreName(miss, name);
    TailCallBuiltin(masm(), MissBuiltin(kind()));
    __ bind(&success);
  }
}


void NamedLoadHandlerCompiler::GenerateLoadCallback(
    Register reg, Handle<ExecutableAccessorInfo> callback) {
  // Insert additional parameters into the stack frame above return address.
  DCHECK(!scratch4().is(reg));
  __ PopReturnAddressTo(scratch4());

  STATIC_ASSERT(PropertyCallbackArguments::kHolderIndex == 0);
  STATIC_ASSERT(PropertyCallbackArguments::kIsolateIndex == 1);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueOffset == 3);
  STATIC_ASSERT(PropertyCallbackArguments::kDataIndex == 4);
  STATIC_ASSERT(PropertyCallbackArguments::kThisIndex == 5);
  STATIC_ASSERT(PropertyCallbackArguments::kArgsLength == 6);
  __ Push(receiver());  // receiver
  if (heap()->InNewSpace(callback->data())) {
    DCHECK(!scratch2().is(reg));
    __ Move(scratch2(), callback);
    __ Push(FieldOperand(scratch2(),
                         ExecutableAccessorInfo::kDataOffset));  // data
  } else {
    __ Push(Handle<Object>(callback->data(), isolate()));
  }
  DCHECK(!kScratchRegister.is(reg));
  __ LoadRoot(kScratchRegister, Heap::kUndefinedValueRootIndex);
  __ Push(kScratchRegister);  // return value
  __ Push(kScratchRegister);  // return value default
  __ PushAddress(ExternalReference::isolate_address(isolate()));
  __ Push(reg);     // holder
  __ Push(name());  // name
  // Save a pointer to where we pushed the arguments pointer.  This will be
  // passed as the const PropertyAccessorInfo& to the C++ callback.

  __ PushReturnAddressFrom(scratch4());

  // Abi for CallApiGetter
  Register api_function_address = ApiGetterDescriptor::function_address();
  Address getter_address = v8::ToCData<Address>(callback->getter());
  __ Move(api_function_address, getter_address, RelocInfo::EXTERNAL_REFERENCE);

  CallApiGetterStub stub(isolate());
  __ TailCallStub(&stub);
}


void NamedLoadHandlerCompiler::GenerateLoadConstant(Handle<Object> value) {
  // Return the constant value.
  __ Move(rax, value);
  __ ret(0);
}


void NamedLoadHandlerCompiler::GenerateLoadInterceptorWithFollowup(
    LookupIterator* it, Register holder_reg) {
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
      __ Push(receiver());
    }
    __ Push(holder_reg);
    __ Push(this->name());

    // Invoke an interceptor.  Note: map checks from receiver to
    // interceptor's holder has been compiled before (see a caller
    // of this method.)
    CompileCallLoadPropertyWithInterceptor(
        masm(), receiver(), holder_reg, this->name(), holder(),
        IC::kLoadPropertyWithInterceptorOnly);

    // Check if interceptor provided a value for property.  If it's
    // the case, return immediately.
    Label interceptor_failed;
    __ CompareRoot(rax, Heap::kNoInterceptorResultSentinelRootIndex);
    __ j(equal, &interceptor_failed);
    frame_scope.GenerateLeaveFrame();
    __ ret(0);

    __ bind(&interceptor_failed);
    __ Pop(this->name());
    __ Pop(holder_reg);
    if (must_preserve_receiver_reg) {
      __ Pop(receiver());
    }

    // Leave the internal frame.
  }

  GenerateLoadPostInterceptor(it, holder_reg);
}


void NamedLoadHandlerCompiler::GenerateLoadInterceptor(Register holder_reg) {
  // Call the runtime system to load the interceptor.
  DCHECK(holder()->HasNamedInterceptor());
  DCHECK(!holder()->GetNamedInterceptor()->getter()->IsUndefined());
  __ PopReturnAddressTo(scratch2());
  PushInterceptorArguments(masm(), receiver(), holder_reg, this->name(),
                           holder());
  __ PushReturnAddressFrom(scratch2());

  ExternalReference ref = ExternalReference(
      IC_Utility(IC::kLoadPropertyWithInterceptor), isolate());
  __ TailCallExternalReference(
      ref, NamedLoadHandlerCompiler::kInterceptorArgsLength, 1);
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreCallback(
    Handle<JSObject> object, Handle<Name> name,
    Handle<ExecutableAccessorInfo> callback) {
  Register holder_reg = Frontend(receiver(), name);

  __ PopReturnAddressTo(scratch1());
  __ Push(receiver());
  __ Push(holder_reg);
  __ Push(callback);  // callback info
  __ Push(name);
  __ Push(value());
  __ PushReturnAddressFrom(scratch1());

  // Do tail-call to the runtime system.
  ExternalReference store_callback_property =
      ExternalReference(IC_Utility(IC::kStoreCallbackProperty), isolate());
  __ TailCallExternalReference(store_callback_property, 5, 1);

  // Return the generated code.
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreInterceptor(
    Handle<Name> name) {
  __ PopReturnAddressTo(scratch1());
  __ Push(receiver());
  __ Push(this->name());
  __ Push(value());
  __ PushReturnAddressFrom(scratch1());

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property = ExternalReference(
      IC_Utility(IC::kStorePropertyWithInterceptor), isolate());
  __ TailCallExternalReference(store_ic_property, 3, 1);

  // Return the generated code.
  return GetCode(kind(), Code::FAST, name);
}


Register NamedStoreHandlerCompiler::value() {
  return StoreDescriptor::ValueRegister();
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadGlobal(
    Handle<PropertyCell> cell, Handle<Name> name, bool is_configurable) {
  Label miss;
  FrontendHeader(receiver(), name, &miss);

  // Get the value from the cell.
  Register result = StoreDescriptor::ValueRegister();
  __ Move(result, cell);
  __ movp(result, FieldOperand(result, PropertyCell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (is_configurable) {
    __ CompareRoot(result, Heap::kTheHoleValueRootIndex);
    __ j(equal, &miss);
  } else if (FLAG_debug_code) {
    __ CompareRoot(result, Heap::kTheHoleValueRootIndex);
    __ Check(not_equal, kDontDeleteCellsCannotContainTheHole);
  }

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->named_load_global_stub(), 1);
  __ ret(0);

  FrontendFooter(name, &miss);

  // Return the generated code.
  return GetCode(kind(), Code::NORMAL, name);
}


#undef __
}
}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
