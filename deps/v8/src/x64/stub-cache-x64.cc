// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8.h"

#if V8_TARGET_ARCH_X64

#include "arguments.h"
#include "ic-inl.h"
#include "codegen.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


static void ProbeTable(Isolate* isolate,
                       MacroAssembler* masm,
                       Code::Flags flags,
                       StubCache::Table table,
                       Register receiver,
                       Register name,
                       // The offset is scaled by 4, based on
                       // kHeapObjectTagSize, which is two bits
                       Register offset) {
  // We need to scale up the pointer by 2 when the offset is scaled by less
  // than the pointer size.
  ASSERT(kPointerSize == kInt64Size
      ? kPointerSizeLog2 == kHeapObjectTagSize + 1
      : kPointerSizeLog2 == kHeapObjectTagSize);
  ScaleFactor scale_factor = kPointerSize == kInt64Size ? times_2 : times_1;

  ASSERT_EQ(3 * kPointerSize, sizeof(StubCache::Entry));
  // The offset register holds the entry offset times four (due to masking
  // and shifting optimizations).
  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  ExternalReference value_offset(isolate->stub_cache()->value_reference(table));
  Label miss;

  // Multiply by 3 because there are 3 fields per entry (name, code, map).
  __ leap(offset, Operand(offset, offset, times_2, 0));

  __ LoadAddress(kScratchRegister, key_offset);

  // Check that the key in the entry matches the name.
  // Multiply entry offset by 16 to get the entry address. Since the
  // offset register already holds the entry offset times four, multiply
  // by a further four.
  __ cmpl(name, Operand(kScratchRegister, offset, scale_factor, 0));
  __ j(not_equal, &miss);

  // Get the map entry from the cache.
  // Use key_offset + kPointerSize * 2, rather than loading map_offset.
  __ movp(kScratchRegister,
          Operand(kScratchRegister, offset, scale_factor, kPointerSize * 2));
  __ cmpp(kScratchRegister, FieldOperand(receiver, HeapObject::kMapOffset));
  __ j(not_equal, &miss);

  // Get the code entry from the cache.
  __ LoadAddress(kScratchRegister, value_offset);
  __ movp(kScratchRegister,
          Operand(kScratchRegister, offset, scale_factor, 0));

  // Check that the flags match what we're looking for.
  __ movl(offset, FieldOperand(kScratchRegister, Code::kFlagsOffset));
  __ andp(offset, Immediate(~Code::kFlagsNotUsedInLookup));
  __ cmpl(offset, Immediate(flags));
  __ j(not_equal, &miss);

#ifdef DEBUG
    if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
      __ jmp(&miss);
    } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
      __ jmp(&miss);
    }
#endif

  // Jump to the first instruction in the code stub.
  __ addp(kScratchRegister, Immediate(Code::kHeaderSize - kHeapObjectTag));
  __ jmp(kScratchRegister);

  __ bind(&miss);
}


void StubCompiler::GenerateDictionaryNegativeLookup(MacroAssembler* masm,
                                                    Label* miss_label,
                                                    Register receiver,
                                                    Handle<Name> name,
                                                    Register scratch0,
                                                    Register scratch1) {
  ASSERT(name->IsUniqueName());
  ASSERT(!receiver.is(scratch0));
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
  NameDictionaryLookupStub::GenerateNegativeLookup(masm,
                                                   miss_label,
                                                   &done,
                                                   properties,
                                                   name,
                                                   scratch1);
  __ bind(&done);
  __ DecrementCounter(counters->negative_lookups_miss(), 1);
}


void StubCache::GenerateProbe(MacroAssembler* masm,
                              Code::Flags flags,
                              Register receiver,
                              Register name,
                              Register scratch,
                              Register extra,
                              Register extra2,
                              Register extra3) {
  Isolate* isolate = masm->isolate();
  Label miss;
  USE(extra);   // The register extra is not used on the X64 platform.
  USE(extra2);  // The register extra2 is not used on the X64 platform.
  USE(extra3);  // The register extra2 is not used on the X64 platform.
  // Make sure that code is valid. The multiplying code relies on the
  // entry size being 3 * kPointerSize.
  ASSERT(sizeof(Entry) == 3 * kPointerSize);

  // Make sure the flags do not name a specific type.
  ASSERT(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  ASSERT(!scratch.is(receiver));
  ASSERT(!scratch.is(name));

  // Check scratch register is valid, extra and extra2 are unused.
  ASSERT(!scratch.is(no_reg));
  ASSERT(extra2.is(no_reg));
  ASSERT(extra3.is(no_reg));

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->megamorphic_stub_cache_probes(), 1);

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ movl(scratch, FieldOperand(name, Name::kHashFieldOffset));
  // Use only the low 32 bits of the map pointer.
  __ addl(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xorp(scratch, Immediate(flags));
  // We mask out the last two bits because they are not part of the hash and
  // they are always 01 for maps.  Also in the two 'and' instructions below.
  __ andp(scratch, Immediate((kPrimaryTableSize - 1) << kHeapObjectTagSize));

  // Probe the primary table.
  ProbeTable(isolate, masm, flags, kPrimary, receiver, name, scratch);

  // Primary miss: Compute hash for secondary probe.
  __ movl(scratch, FieldOperand(name, Name::kHashFieldOffset));
  __ addl(scratch, FieldOperand(receiver, HeapObject::kMapOffset));
  __ xorp(scratch, Immediate(flags));
  __ andp(scratch, Immediate((kPrimaryTableSize - 1) << kHeapObjectTagSize));
  __ subl(scratch, name);
  __ addl(scratch, Immediate(flags));
  __ andp(scratch, Immediate((kSecondaryTableSize - 1) << kHeapObjectTagSize));

  // Probe the secondary table.
  ProbeTable(isolate, masm, flags, kSecondary, receiver, name, scratch);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1);
}


void StubCompiler::GenerateLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                       int index,
                                                       Register prototype) {
  // Load the global or builtins object from the current context.
  __ movp(prototype,
          Operand(rsi, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  // Load the native context from the global or builtins object.
  __ movp(prototype,
          FieldOperand(prototype, GlobalObject::kNativeContextOffset));
  // Load the function from the native context.
  __ movp(prototype, Operand(prototype, Context::SlotOffset(index)));
  // Load the initial map.  The global functions all have initial maps.
  __ movp(prototype,
          FieldOperand(prototype, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the prototype from the initial map.
  __ movp(prototype, FieldOperand(prototype, Map::kPrototypeOffset));
}


void StubCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm,
    int index,
    Register prototype,
    Label* miss) {
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


void StubCompiler::GenerateLoadArrayLength(MacroAssembler* masm,
                                           Register receiver,
                                           Register scratch,
                                           Label* miss_label) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss_label);

  // Check that the object is a JS array.
  __ CmpObjectType(receiver, JS_ARRAY_TYPE, scratch);
  __ j(not_equal, miss_label);

  // Load length directly from the JS array.
  __ movp(rax, FieldOperand(receiver, JSArray::kLengthOffset));
  __ ret(0);
}


void StubCompiler::GenerateLoadFunctionPrototype(MacroAssembler* masm,
                                                 Register receiver,
                                                 Register result,
                                                 Register scratch,
                                                 Label* miss_label) {
  __ TryGetFunctionPrototype(receiver, result, miss_label);
  if (!result.is(rax)) __ movp(rax, result);
  __ ret(0);
}


void StubCompiler::GenerateFastPropertyLoad(MacroAssembler* masm,
                                            Register dst,
                                            Register src,
                                            bool inobject,
                                            int index,
                                            Representation representation) {
  ASSERT(!representation.IsDouble());
  int offset = index * kPointerSize;
  if (!inobject) {
    // Calculate the offset into the properties array.
    offset = offset + FixedArray::kHeaderSize;
    __ movp(dst, FieldOperand(src, JSObject::kPropertiesOffset));
    src = dst;
  }
  __ movp(dst, FieldOperand(src, offset));
}


static void PushInterceptorArguments(MacroAssembler* masm,
                                     Register receiver,
                                     Register holder,
                                     Register name,
                                     Handle<JSObject> holder_obj) {
  STATIC_ASSERT(StubCache::kInterceptorArgsNameIndex == 0);
  STATIC_ASSERT(StubCache::kInterceptorArgsInfoIndex == 1);
  STATIC_ASSERT(StubCache::kInterceptorArgsThisIndex == 2);
  STATIC_ASSERT(StubCache::kInterceptorArgsHolderIndex == 3);
  STATIC_ASSERT(StubCache::kInterceptorArgsLength == 4);
  __ Push(name);
  Handle<InterceptorInfo> interceptor(holder_obj->GetNamedInterceptor());
  ASSERT(!masm->isolate()->heap()->InNewSpace(*interceptor));
  __ Move(kScratchRegister, interceptor);
  __ Push(kScratchRegister);
  __ Push(receiver);
  __ Push(holder);
}


static void CompileCallLoadPropertyWithInterceptor(
    MacroAssembler* masm,
    Register receiver,
    Register holder,
    Register name,
    Handle<JSObject> holder_obj,
    IC::UtilityId id) {
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);
  __ CallExternalReference(
      ExternalReference(IC_Utility(id), masm->isolate()),
      StubCache::kInterceptorArgsLength);
}


// Generate call to api function.
void StubCompiler::GenerateFastApiCall(MacroAssembler* masm,
                                       const CallOptimization& optimization,
                                       Handle<Map> receiver_map,
                                       Register receiver,
                                       Register scratch_in,
                                       bool is_store,
                                       int argc,
                                       Register* values) {
  ASSERT(optimization.is_simple_api_call());

  __ PopReturnAddressTo(scratch_in);
  // receiver
  __ Push(receiver);
  // Write the arguments to stack frame.
  for (int i = 0; i < argc; i++) {
    Register arg = values[argc-1-i];
    ASSERT(!receiver.is(arg));
    ASSERT(!scratch_in.is(arg));
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
  Handle<JSObject> api_holder = optimization.LookupHolderOfExpectedType(
      receiver_map,
      &holder_lookup);
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
  __ Move(
      api_function_address, function_address, RelocInfo::EXTERNAL_REFERENCE);

  // Jump to stub.
  CallApiFunctionStub stub(isolate, is_store, call_data_undefined, argc);
  __ TailCallStub(&stub);
}


void StoreStubCompiler::GenerateRestoreName(MacroAssembler* masm,
                                            Label* label,
                                            Handle<Name> name) {
  if (!label->is_unused()) {
    __ bind(label);
    __ Move(this->name(), name);
  }
}


void StubCompiler::GenerateCheckPropertyCell(MacroAssembler* masm,
                                             Handle<JSGlobalObject> global,
                                             Handle<Name> name,
                                             Register scratch,
                                             Label* miss) {
  Handle<PropertyCell> cell =
      JSGlobalObject::EnsurePropertyCell(global, name);
  ASSERT(cell->value()->IsTheHole());
  __ Move(scratch, cell);
  __ Cmp(FieldOperand(scratch, Cell::kValueOffset),
         masm->isolate()->factory()->the_hole_value());
  __ j(not_equal, miss);
}


void StoreStubCompiler::GenerateNegativeHolderLookup(
    MacroAssembler* masm,
    Handle<JSObject> holder,
    Register holder_reg,
    Handle<Name> name,
    Label* miss) {
  if (holder->IsJSGlobalObject()) {
    GenerateCheckPropertyCell(
        masm, Handle<JSGlobalObject>::cast(holder), name, scratch1(), miss);
  } else if (!holder->HasFastProperties() && !holder->IsJSGlobalProxy()) {
    GenerateDictionaryNegativeLookup(
        masm, miss, holder_reg, name, scratch1(), scratch2());
  }
}


// Receiver_reg is preserved on jumps to miss_label, but may be destroyed if
// store is successful.
void StoreStubCompiler::GenerateStoreTransition(MacroAssembler* masm,
                                                Handle<JSObject> object,
                                                LookupResult* lookup,
                                                Handle<Map> transition,
                                                Handle<Name> name,
                                                Register receiver_reg,
                                                Register storage_reg,
                                                Register value_reg,
                                                Register scratch1,
                                                Register scratch2,
                                                Register unused,
                                                Label* miss_label,
                                                Label* slow) {
  int descriptor = transition->LastAdded();
  DescriptorArray* descriptors = transition->instance_descriptors();
  PropertyDetails details = descriptors->GetDetails(descriptor);
  Representation representation = details.representation();
  ASSERT(!representation.IsNone());

  if (details.type() == CONSTANT) {
    Handle<Object> constant(descriptors->GetValue(descriptor), masm->isolate());
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
    __ AllocateHeapNumber(storage_reg, scratch1, slow);

    __ JumpIfNotSmi(value_reg, &heap_number);
    __ SmiToInteger32(scratch1, value_reg);
    __ Cvtlsi2sd(xmm0, scratch1);
    __ jmp(&do_store);

    __ bind(&heap_number);
    __ CheckMap(value_reg, masm->isolate()->factory()->heap_number_map(),
                miss_label, DONT_DO_SMI_CHECK);
    __ movsd(xmm0, FieldOperand(value_reg, HeapNumber::kValueOffset));

    __ bind(&do_store);
    __ movsd(FieldOperand(storage_reg, HeapNumber::kValueOffset), xmm0);
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  // Perform map transition for the receiver if necessary.
  if (details.type() == FIELD &&
      object->map()->unused_property_fields() == 0) {
    // The properties must be extended before we can store the value.
    // We jump to a runtime call that extends the properties array.
    __ PopReturnAddressTo(scratch1);
    __ Push(receiver_reg);
    __ Push(transition);
    __ Push(value_reg);
    __ PushReturnAddressFrom(scratch1);
    __ TailCallExternalReference(
        ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage),
                          masm->isolate()),
        3,
        1);
    return;
  }

  // Update the map of the object.
  __ Move(scratch1, transition);
  __ movp(FieldOperand(receiver_reg, HeapObject::kMapOffset), scratch1);

  // Update the write barrier for the map field.
  __ RecordWriteField(receiver_reg,
                      HeapObject::kMapOffset,
                      scratch1,
                      scratch2,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  if (details.type() == CONSTANT) {
    ASSERT(value_reg.is(rax));
    __ ret(0);
    return;
  }

  int index = transition->instance_descriptors()->GetFieldIndex(
      transition->LastAdded());

  // Adjust for the number of properties stored in the object. Even in the
  // face of a transition we can use the old map here because the size of the
  // object and the number of in-object properties is not going to change.
  index -= object->map()->inobject_properties();

  // TODO(verwaest): Share this code as a code stub.
  SmiCheck smi_check = representation.IsTagged()
      ? INLINE_SMI_CHECK : OMIT_SMI_CHECK;
  if (index < 0) {
    // Set the property straight into the object.
    int offset = object->map()->instance_size() + (index * kPointerSize);
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
      __ RecordWriteField(
          receiver_reg, offset, storage_reg, scratch1, kDontSaveFPRegs,
          EMIT_REMEMBERED_SET, smi_check);
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
      __ RecordWriteField(
          scratch1, offset, storage_reg, receiver_reg, kDontSaveFPRegs,
          EMIT_REMEMBERED_SET, smi_check);
    }
  }

  // Return the value (register rax).
  ASSERT(value_reg.is(rax));
  __ ret(0);
}


// Both name_reg and receiver_reg are preserved on jumps to miss_label,
// but may be destroyed if store is successful.
void StoreStubCompiler::GenerateStoreField(MacroAssembler* masm,
                                           Handle<JSObject> object,
                                           LookupResult* lookup,
                                           Register receiver_reg,
                                           Register name_reg,
                                           Register value_reg,
                                           Register scratch1,
                                           Register scratch2,
                                           Label* miss_label) {
  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  int index = lookup->GetFieldIndex().field_index();

  // Adjust for the number of properties stored in the object. Even in the
  // face of a transition we can use the old map here because the size of the
  // object and the number of in-object properties is not going to change.
  index -= object->map()->inobject_properties();

  Representation representation = lookup->representation();
  ASSERT(!representation.IsNone());
  if (representation.IsSmi()) {
    __ JumpIfNotSmi(value_reg, miss_label);
  } else if (representation.IsHeapObject()) {
    __ JumpIfSmi(value_reg, miss_label);
    HeapType* field_type = lookup->GetFieldType();
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
    // Load the double storage.
    if (index < 0) {
      int offset = object->map()->instance_size() + (index * kPointerSize);
      __ movp(scratch1, FieldOperand(receiver_reg, offset));
    } else {
      __ movp(scratch1,
              FieldOperand(receiver_reg, JSObject::kPropertiesOffset));
      int offset = index * kPointerSize + FixedArray::kHeaderSize;
      __ movp(scratch1, FieldOperand(scratch1, offset));
    }

    // Store the value into the storage.
    Label do_store, heap_number;
    __ JumpIfNotSmi(value_reg, &heap_number);
    __ SmiToInteger32(scratch2, value_reg);
    __ Cvtlsi2sd(xmm0, scratch2);
    __ jmp(&do_store);

    __ bind(&heap_number);
    __ CheckMap(value_reg, masm->isolate()->factory()->heap_number_map(),
                miss_label, DONT_DO_SMI_CHECK);
    __ movsd(xmm0, FieldOperand(value_reg, HeapNumber::kValueOffset));
    __ bind(&do_store);
    __ movsd(FieldOperand(scratch1, HeapNumber::kValueOffset), xmm0);
    // Return the value (register rax).
    ASSERT(value_reg.is(rax));
    __ ret(0);
    return;
  }

  // TODO(verwaest): Share this code as a code stub.
  SmiCheck smi_check = representation.IsTagged()
      ? INLINE_SMI_CHECK : OMIT_SMI_CHECK;
  if (index < 0) {
    // Set the property straight into the object.
    int offset = object->map()->instance_size() + (index * kPointerSize);
    __ movp(FieldOperand(receiver_reg, offset), value_reg);

    if (!representation.IsSmi()) {
      // Update the write barrier for the array address.
      // Pass the value being stored in the now unused name_reg.
      __ movp(name_reg, value_reg);
      __ RecordWriteField(
          receiver_reg, offset, name_reg, scratch1, kDontSaveFPRegs,
          EMIT_REMEMBERED_SET, smi_check);
    }
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array (optimistically).
    __ movp(scratch1, FieldOperand(receiver_reg, JSObject::kPropertiesOffset));
    __ movp(FieldOperand(scratch1, offset), value_reg);

    if (!representation.IsSmi()) {
      // Update the write barrier for the array address.
      // Pass the value being stored in the now unused name_reg.
      __ movp(name_reg, value_reg);
      __ RecordWriteField(
          scratch1, offset, name_reg, receiver_reg, kDontSaveFPRegs,
          EMIT_REMEMBERED_SET, smi_check);
    }
  }

  // Return the value (register rax).
  ASSERT(value_reg.is(rax));
  __ ret(0);
}


void StubCompiler::GenerateTailCall(MacroAssembler* masm, Handle<Code> code) {
  __ jmp(code, RelocInfo::CODE_TARGET);
}


#undef __
#define __ ACCESS_MASM((masm()))


Register StubCompiler::CheckPrototypes(Handle<HeapType> type,
                                       Register object_reg,
                                       Handle<JSObject> holder,
                                       Register holder_reg,
                                       Register scratch1,
                                       Register scratch2,
                                       Handle<Name> name,
                                       Label* miss,
                                       PrototypeCheckType check) {
  Handle<Map> receiver_map(IC::TypeToMap(*type, isolate()));

  // Make sure there's no overlap between holder and object registers.
  ASSERT(!scratch1.is(object_reg) && !scratch1.is(holder_reg));
  ASSERT(!scratch2.is(object_reg) && !scratch2.is(holder_reg)
         && !scratch2.is(scratch1));

  // Keep track of the current object in register reg.  On the first
  // iteration, reg is an alias for object_reg, on later iterations,
  // it is an alias for holder_reg.
  Register reg = object_reg;
  int depth = 0;

  Handle<JSObject> current = Handle<JSObject>::null();
  if (type->IsConstant()) {
    current = Handle<JSObject>::cast(type->AsConstant()->Value());
  }
  Handle<JSObject> prototype = Handle<JSObject>::null();
  Handle<Map> current_map = receiver_map;
  Handle<Map> holder_map(holder->map());
  // Traverse the prototype chain and check the maps in the prototype chain for
  // fast and global objects or do negative lookup for normal objects.
  while (!current_map.is_identical_to(holder_map)) {
    ++depth;

    // Only global objects and objects that do not require access
    // checks are allowed in stubs.
    ASSERT(current_map->IsJSGlobalProxyMap() ||
           !current_map->is_access_check_needed());

    prototype = handle(JSObject::cast(current_map->prototype()));
    if (current_map->is_dictionary_map() &&
        !current_map->IsJSGlobalObjectMap() &&
        !current_map->IsJSGlobalProxyMap()) {
      if (!name->IsUniqueName()) {
        ASSERT(name->IsString());
        name = factory()->InternalizeString(Handle<String>::cast(name));
      }
      ASSERT(current.is_null() ||
             current->property_dictionary()->FindEntry(name) ==
             NameDictionary::kNotFound);

      GenerateDictionaryNegativeLookup(masm(), miss, reg, name,
                                       scratch1, scratch2);

      __ movp(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // From now on the object will be in holder_reg.
      __ movp(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
    } else {
      bool in_new_space = heap()->InNewSpace(*prototype);
      if (in_new_space) {
        // Save the map in scratch1 for later.
        __ movp(scratch1, FieldOperand(reg, HeapObject::kMapOffset));
      }
      if (depth != 1 || check == CHECK_ALL_MAPS) {
        __ CheckMap(reg, current_map, miss, DONT_DO_SMI_CHECK);
      }

      // Check access rights to the global object.  This has to happen after
      // the map check so that we know that the object is actually a global
      // object.
      if (current_map->IsJSGlobalProxyMap()) {
        __ CheckAccessGlobalProxy(reg, scratch2, miss);
      } else if (current_map->IsJSGlobalObjectMap()) {
        GenerateCheckPropertyCell(
            masm(), Handle<JSGlobalObject>::cast(current), name,
            scratch2, miss);
      }
      reg = holder_reg;  // From now on the object will be in holder_reg.

      if (in_new_space) {
        // The prototype is in new space; we cannot store a reference to it
        // in the code.  Load it from the map.
        __ movp(reg, FieldOperand(scratch1, Map::kPrototypeOffset));
      } else {
        // The prototype is in old space; load it directly.
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
  ASSERT(current_map->IsJSGlobalProxyMap() ||
         !current_map->is_access_check_needed());
  if (current_map->IsJSGlobalProxyMap()) {
    __ CheckAccessGlobalProxy(reg, scratch1, miss);
  }

  // Return the register containing the holder.
  return reg;
}


void LoadStubCompiler::HandlerFrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ jmp(&success);
    __ bind(miss);
    TailCallBuiltin(masm(), MissBuiltin(kind()));
    __ bind(&success);
  }
}


void StoreStubCompiler::HandlerFrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ jmp(&success);
    GenerateRestoreName(masm(), miss, name);
    TailCallBuiltin(masm(), MissBuiltin(kind()));
    __ bind(&success);
  }
}


Register LoadStubCompiler::CallbackHandlerFrontend(
    Handle<HeapType> type,
    Register object_reg,
    Handle<JSObject> holder,
    Handle<Name> name,
    Handle<Object> callback) {
  Label miss;

  Register reg = HandlerFrontendHeader(type, object_reg, holder, name, &miss);

  if (!holder->HasFastProperties() && !holder->IsJSGlobalObject()) {
    ASSERT(!reg.is(scratch2()));
    ASSERT(!reg.is(scratch3()));
    ASSERT(!reg.is(scratch4()));

    // Load the properties dictionary.
    Register dictionary = scratch4();
    __ movp(dictionary, FieldOperand(reg, JSObject::kPropertiesOffset));

    // Probe the dictionary.
    Label probe_done;
    NameDictionaryLookupStub::GeneratePositiveLookup(masm(),
                                                     &miss,
                                                     &probe_done,
                                                     dictionary,
                                                     this->name(),
                                                     scratch2(),
                                                     scratch3());
    __ bind(&probe_done);

    // If probing finds an entry in the dictionary, scratch3 contains the
    // index into the dictionary. Check that the value is the callback.
    Register index = scratch3();
    const int kElementsStartOffset =
        NameDictionary::kHeaderSize +
        NameDictionary::kElementsStartIndex * kPointerSize;
    const int kValueOffset = kElementsStartOffset + kPointerSize;
    __ movp(scratch2(),
            Operand(dictionary, index, times_pointer_size,
                    kValueOffset - kHeapObjectTag));
    __ Move(scratch3(), callback, RelocInfo::EMBEDDED_OBJECT);
    __ cmpp(scratch2(), scratch3());
    __ j(not_equal, &miss);
  }

  HandlerFrontendFooter(name, &miss);
  return reg;
}


void LoadStubCompiler::GenerateLoadField(Register reg,
                                         Handle<JSObject> holder,
                                         PropertyIndex field,
                                         Representation representation) {
  if (!reg.is(receiver())) __ movp(receiver(), reg);
  if (kind() == Code::LOAD_IC) {
    LoadFieldStub stub(isolate(),
                       field.is_inobject(holder),
                       field.translate(holder),
                       representation);
    GenerateTailCall(masm(), stub.GetCode());
  } else {
    KeyedLoadFieldStub stub(isolate(),
                            field.is_inobject(holder),
                            field.translate(holder),
                            representation);
    GenerateTailCall(masm(), stub.GetCode());
  }
}


void LoadStubCompiler::GenerateLoadCallback(
    Register reg,
    Handle<ExecutableAccessorInfo> callback) {
  // Insert additional parameters into the stack frame above return address.
  ASSERT(!scratch4().is(reg));
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
    ASSERT(!scratch2().is(reg));
    __ Move(scratch2(), callback);
    __ Push(FieldOperand(scratch2(),
                         ExecutableAccessorInfo::kDataOffset));  // data
  } else {
    __ Push(Handle<Object>(callback->data(), isolate()));
  }
  ASSERT(!kScratchRegister.is(reg));
  __ LoadRoot(kScratchRegister, Heap::kUndefinedValueRootIndex);
  __ Push(kScratchRegister);  // return value
  __ Push(kScratchRegister);  // return value default
  __ PushAddress(ExternalReference::isolate_address(isolate()));
  __ Push(reg);  // holder
  __ Push(name());  // name
  // Save a pointer to where we pushed the arguments pointer.  This will be
  // passed as the const PropertyAccessorInfo& to the C++ callback.

  __ PushReturnAddressFrom(scratch4());

  // Abi for CallApiGetter
  Register api_function_address = r8;
  Address getter_address = v8::ToCData<Address>(callback->getter());
  __ Move(api_function_address, getter_address, RelocInfo::EXTERNAL_REFERENCE);

  CallApiGetterStub stub(isolate());
  __ TailCallStub(&stub);
}


void LoadStubCompiler::GenerateLoadConstant(Handle<Object> value) {
  // Return the constant value.
  __ Move(rax, value);
  __ ret(0);
}


void LoadStubCompiler::GenerateLoadInterceptor(
    Register holder_reg,
    Handle<Object> object,
    Handle<JSObject> interceptor_holder,
    LookupResult* lookup,
    Handle<Name> name) {
  ASSERT(interceptor_holder->HasNamedInterceptor());
  ASSERT(!interceptor_holder->GetNamedInterceptor()->getter()->IsUndefined());

  // So far the most popular follow ups for interceptor loads are FIELD
  // and CALLBACKS, so inline only them, other cases may be added
  // later.
  bool compile_followup_inline = false;
  if (lookup->IsFound() && lookup->IsCacheable()) {
    if (lookup->IsField()) {
      compile_followup_inline = true;
    } else if (lookup->type() == CALLBACKS &&
               lookup->GetCallbackObject()->IsExecutableAccessorInfo()) {
      ExecutableAccessorInfo* callback =
          ExecutableAccessorInfo::cast(lookup->GetCallbackObject());
      compile_followup_inline = callback->getter() != NULL &&
          callback->IsCompatibleReceiver(*object);
    }
  }

  if (compile_followup_inline) {
    // Compile the interceptor call, followed by inline code to load the
    // property from further up the prototype chain if the call fails.
    // Check that the maps haven't changed.
    ASSERT(holder_reg.is(receiver()) || holder_reg.is(scratch1()));

    // Preserve the receiver register explicitly whenever it is different from
    // the holder and it is needed should the interceptor return without any
    // result. The CALLBACKS case needs the receiver to be passed into C++ code,
    // the FIELD case might cause a miss during the prototype check.
    bool must_perfrom_prototype_check = *interceptor_holder != lookup->holder();
    bool must_preserve_receiver_reg = !receiver().is(holder_reg) &&
        (lookup->type() == CALLBACKS || must_perfrom_prototype_check);

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
          masm(), receiver(), holder_reg, this->name(), interceptor_holder,
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

    GenerateLoadPostInterceptor(holder_reg, interceptor_holder, name, lookup);
  } else {  // !compile_followup_inline
    // Call the runtime system to load the interceptor.
    // Check that the maps haven't changed.
    __ PopReturnAddressTo(scratch2());
    PushInterceptorArguments(masm(), receiver(), holder_reg,
                             this->name(), interceptor_holder);
    __ PushReturnAddressFrom(scratch2());

    ExternalReference ref = ExternalReference(
        IC_Utility(IC::kLoadPropertyWithInterceptorForLoad), isolate());
    __ TailCallExternalReference(ref, StubCache::kInterceptorArgsLength, 1);
  }
}


Handle<Code> StoreStubCompiler::CompileStoreCallback(
    Handle<JSObject> object,
    Handle<JSObject> holder,
    Handle<Name> name,
    Handle<ExecutableAccessorInfo> callback) {
  Register holder_reg = HandlerFrontend(
      IC::CurrentTypeOf(object, isolate()), receiver(), holder, name);

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


#undef __
#define __ ACCESS_MASM(masm)


void StoreStubCompiler::GenerateStoreViaSetter(
    MacroAssembler* masm,
    Handle<HeapType> type,
    Register receiver,
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
                FieldOperand(receiver, JSGlobalObject::kGlobalReceiverOffset));
      }
      __ Push(receiver);
      __ Push(value());
      ParameterCount actual(1);
      ParameterCount expected(setter);
      __ InvokeFunction(setter, expected, actual,
                        CALL_FUNCTION, NullCallWrapper());
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


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> StoreStubCompiler::CompileStoreInterceptor(
    Handle<JSObject> object,
    Handle<Name> name) {
  __ PopReturnAddressTo(scratch1());
  __ Push(receiver());
  __ Push(this->name());
  __ Push(value());
  __ PushReturnAddressFrom(scratch1());

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property =
      ExternalReference(IC_Utility(IC::kStoreInterceptorProperty), isolate());
  __ TailCallExternalReference(store_ic_property, 3, 1);

  // Return the generated code.
  return GetCode(kind(), Code::FAST, name);
}


void StoreStubCompiler::GenerateStoreArrayLength() {
  // Prepare tail call to StoreIC_ArrayLength.
  __ PopReturnAddressTo(scratch1());
  __ Push(receiver());
  __ Push(value());
  __ PushReturnAddressFrom(scratch1());

  ExternalReference ref =
      ExternalReference(IC_Utility(IC::kStoreIC_ArrayLength),
                        masm()->isolate());
  __ TailCallExternalReference(ref, 2, 1);
}


Handle<Code> KeyedStoreStubCompiler::CompileStorePolymorphic(
    MapHandleList* receiver_maps,
    CodeHandleList* handler_stubs,
    MapHandleList* transitioned_maps) {
  Label miss;
  __ JumpIfSmi(receiver(), &miss, Label::kNear);

  __ movp(scratch1(), FieldOperand(receiver(), HeapObject::kMapOffset));
  int receiver_count = receiver_maps->length();
  for (int i = 0; i < receiver_count; ++i) {
    // Check map and tail call if there's a match
    __ Cmp(scratch1(), receiver_maps->at(i));
    if (transitioned_maps->at(i).is_null()) {
      __ j(equal, handler_stubs->at(i), RelocInfo::CODE_TARGET);
    } else {
      Label next_map;
      __ j(not_equal, &next_map, Label::kNear);
      __ Move(transition_map(),
              transitioned_maps->at(i),
              RelocInfo::EMBEDDED_OBJECT);
      __ jmp(handler_stubs->at(i), RelocInfo::CODE_TARGET);
      __ bind(&next_map);
    }
  }

  __ bind(&miss);

  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  return GetICCode(
      kind(), Code::NORMAL, factory()->empty_string(), POLYMORPHIC);
}


Handle<Code> LoadStubCompiler::CompileLoadNonexistent(Handle<HeapType> type,
                                                      Handle<JSObject> last,
                                                      Handle<Name> name) {
  NonexistentHandlerFrontend(type, last, name);

  // Return undefined if maps of the full prototype chain are still the
  // same and no global property with this name contains a value.
  __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
  __ ret(0);

  // Return the generated code.
  return GetCode(kind(), Code::FAST, name);
}


Register* LoadStubCompiler::registers() {
  // receiver, name, scratch1, scratch2, scratch3, scratch4.
  static Register registers[] = { rax, rcx, rdx, rbx, rdi, r8 };
  return registers;
}


Register* KeyedLoadStubCompiler::registers() {
  // receiver, name, scratch1, scratch2, scratch3, scratch4.
  static Register registers[] = { rdx, rax, rbx, rcx, rdi, r8 };
  return registers;
}


Register StoreStubCompiler::value() {
  return rax;
}


Register* StoreStubCompiler::registers() {
  // receiver, name, scratch1, scratch2, scratch3.
  static Register registers[] = { rdx, rcx, rbx, rdi, r8 };
  return registers;
}


Register* KeyedStoreStubCompiler::registers() {
  // receiver, name, scratch1, scratch2, scratch3.
  static Register registers[] = { rdx, rcx, rbx, rdi, r8 };
  return registers;
}


#undef __
#define __ ACCESS_MASM(masm)


void LoadStubCompiler::GenerateLoadViaGetter(MacroAssembler* masm,
                                             Handle<HeapType> type,
                                             Register receiver,
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
                FieldOperand(receiver, JSGlobalObject::kGlobalReceiverOffset));
      }
      __ Push(receiver);
      ParameterCount actual(0);
      ParameterCount expected(getter);
      __ InvokeFunction(getter, expected, actual,
                        CALL_FUNCTION, NullCallWrapper());
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


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> LoadStubCompiler::CompileLoadGlobal(
    Handle<HeapType> type,
    Handle<GlobalObject> global,
    Handle<PropertyCell> cell,
    Handle<Name> name,
    bool is_dont_delete) {
  Label miss;
  // TODO(verwaest): Directly store to rax. Currently we cannot do this, since
  // rax is used as receiver(), which we would otherwise clobber before a
  // potential miss.
  HandlerFrontendHeader(type, receiver(), global, name, &miss);

  // Get the value from the cell.
  __ Move(rbx, cell);
  __ movp(rbx, FieldOperand(rbx, PropertyCell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (!is_dont_delete) {
    __ CompareRoot(rbx, Heap::kTheHoleValueRootIndex);
    __ j(equal, &miss);
  } else if (FLAG_debug_code) {
    __ CompareRoot(rbx, Heap::kTheHoleValueRootIndex);
    __ Check(not_equal, kDontDeleteCellsCannotContainTheHole);
  }

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->named_load_global_stub(), 1);
  __ movp(rax, rbx);
  __ ret(0);

  HandlerFrontendFooter(name, &miss);

  // Return the generated code.
  return GetCode(kind(), Code::NORMAL, name);
}


Handle<Code> BaseLoadStoreStubCompiler::CompilePolymorphicIC(
    TypeHandleList* types,
    CodeHandleList* handlers,
    Handle<Name> name,
    Code::StubType type,
    IcCheckType check) {
  Label miss;

  if (check == PROPERTY &&
      (kind() == Code::KEYED_LOAD_IC || kind() == Code::KEYED_STORE_IC)) {
    __ Cmp(this->name(), name);
    __ j(not_equal, &miss);
  }

  Label number_case;
  Label* smi_target = IncludesNumberType(types) ? &number_case : &miss;
  __ JumpIfSmi(receiver(), smi_target);

  Register map_reg = scratch1();
  __ movp(map_reg, FieldOperand(receiver(), HeapObject::kMapOffset));
  int receiver_count = types->length();
  int number_of_handled_maps = 0;
  for (int current = 0; current < receiver_count; ++current) {
    Handle<HeapType> type = types->at(current);
    Handle<Map> map = IC::TypeToMap(*type, isolate());
    if (!map->is_deprecated()) {
      number_of_handled_maps++;
      // Check map and tail call if there's a match
      __ Cmp(map_reg, map);
      if (type->Is(HeapType::Number())) {
        ASSERT(!number_case.is_unused());
        __ bind(&number_case);
      }
      __ j(equal, handlers->at(current), RelocInfo::CODE_TARGET);
    }
  }
  ASSERT(number_of_handled_maps > 0);

  __  bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  InlineCacheState state =
      number_of_handled_maps > 1 ? POLYMORPHIC : MONOMORPHIC;
  return GetICCode(kind(), type, name, state);
}


#undef __
#define __ ACCESS_MASM(masm)


void KeyedLoadStubCompiler::GenerateLoadDictionaryElement(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label slow, miss;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  __ JumpIfNotSmi(rax, &miss);
  __ SmiToInteger32(rbx, rax);
  __ movp(rcx, FieldOperand(rdx, JSObject::kElementsOffset));

  // Check whether the elements is a number dictionary.
  // rdx: receiver
  // rax: key
  // rbx: key as untagged int32
  // rcx: elements
  __ LoadFromNumberDictionary(&slow, rcx, rax, rbx, r9, rdi, rax);
  __ ret(0);

  __ bind(&slow);
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  TailCallBuiltin(masm, Builtins::kKeyedLoadIC_Slow);

  __ bind(&miss);
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  TailCallBuiltin(masm, Builtins::kKeyedLoadIC_Miss);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
