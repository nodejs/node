// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#if V8_TARGET_ARCH_MIPS

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
                       // Number of the cache entry, not scaled.
                       Register offset,
                       Register scratch,
                       Register scratch2,
                       Register offset_scratch) {
  ExternalReference key_offset(isolate->stub_cache()->key_reference(table));
  ExternalReference value_offset(isolate->stub_cache()->value_reference(table));
  ExternalReference map_offset(isolate->stub_cache()->map_reference(table));

  uint32_t key_off_addr = reinterpret_cast<uint32_t>(key_offset.address());
  uint32_t value_off_addr = reinterpret_cast<uint32_t>(value_offset.address());
  uint32_t map_off_addr = reinterpret_cast<uint32_t>(map_offset.address());

  // Check the relative positions of the address fields.
  ASSERT(value_off_addr > key_off_addr);
  ASSERT((value_off_addr - key_off_addr) % 4 == 0);
  ASSERT((value_off_addr - key_off_addr) < (256 * 4));
  ASSERT(map_off_addr > key_off_addr);
  ASSERT((map_off_addr - key_off_addr) % 4 == 0);
  ASSERT((map_off_addr - key_off_addr) < (256 * 4));

  Label miss;
  Register base_addr = scratch;
  scratch = no_reg;

  // Multiply by 3 because there are 3 fields per entry (name, code, map).
  __ sll(offset_scratch, offset, 1);
  __ Addu(offset_scratch, offset_scratch, offset);

  // Calculate the base address of the entry.
  __ li(base_addr, Operand(key_offset));
  __ sll(at, offset_scratch, kPointerSizeLog2);
  __ Addu(base_addr, base_addr, at);

  // Check that the key in the entry matches the name.
  __ lw(at, MemOperand(base_addr, 0));
  __ Branch(&miss, ne, name, Operand(at));

  // Check the map matches.
  __ lw(at, MemOperand(base_addr, map_off_addr - key_off_addr));
  __ lw(scratch2, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Branch(&miss, ne, at, Operand(scratch2));

  // Get the code entry from the cache.
  Register code = scratch2;
  scratch2 = no_reg;
  __ lw(code, MemOperand(base_addr, value_off_addr - key_off_addr));

  // Check that the flags match what we're looking for.
  Register flags_reg = base_addr;
  base_addr = no_reg;
  __ lw(flags_reg, FieldMemOperand(code, Code::kFlagsOffset));
  __ And(flags_reg, flags_reg, Operand(~Code::kFlagsNotUsedInLookup));
  __ Branch(&miss, ne, flags_reg, Operand(flags));

#ifdef DEBUG
    if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
      __ jmp(&miss);
    } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
      __ jmp(&miss);
    }
#endif

  // Jump to the first instruction in the code stub.
  __ Addu(at, code, Operand(Code::kHeaderSize - kHeapObjectTag));
  __ Jump(at);

  // Miss: fall through.
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
  __ Branch(miss_label, lt, scratch0, Operand(FIRST_SPEC_OBJECT_TYPE));

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


  NameDictionaryLookupStub::GenerateNegativeLookup(masm,
                                                   miss_label,
                                                   &done,
                                                   receiver,
                                                   properties,
                                                   name,
                                                   scratch1);
  __ bind(&done);
  __ DecrementCounter(counters->negative_lookups_miss(), 1, scratch0, scratch1);
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

  // Make sure that code is valid. The multiplying code relies on the
  // entry size being 12.
  ASSERT(sizeof(Entry) == 12);

  // Make sure the flags does not name a specific type.
  ASSERT(Code::ExtractTypeFromFlags(flags) == 0);

  // Make sure that there are no register conflicts.
  ASSERT(!scratch.is(receiver));
  ASSERT(!scratch.is(name));
  ASSERT(!extra.is(receiver));
  ASSERT(!extra.is(name));
  ASSERT(!extra.is(scratch));
  ASSERT(!extra2.is(receiver));
  ASSERT(!extra2.is(name));
  ASSERT(!extra2.is(scratch));
  ASSERT(!extra2.is(extra));

  // Check register validity.
  ASSERT(!scratch.is(no_reg));
  ASSERT(!extra.is(no_reg));
  ASSERT(!extra2.is(no_reg));
  ASSERT(!extra3.is(no_reg));

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->megamorphic_stub_cache_probes(), 1,
                      extra2, extra3);

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Get the map of the receiver and compute the hash.
  __ lw(scratch, FieldMemOperand(name, Name::kHashFieldOffset));
  __ lw(at, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Addu(scratch, scratch, at);
  uint32_t mask = kPrimaryTableSize - 1;
  // We shift out the last two bits because they are not part of the hash and
  // they are always 01 for maps.
  __ srl(scratch, scratch, kHeapObjectTagSize);
  __ Xor(scratch, scratch, Operand((flags >> kHeapObjectTagSize) & mask));
  __ And(scratch, scratch, Operand(mask));

  // Probe the primary table.
  ProbeTable(isolate,
             masm,
             flags,
             kPrimary,
             receiver,
             name,
             scratch,
             extra,
             extra2,
             extra3);

  // Primary miss: Compute hash for secondary probe.
  __ srl(at, name, kHeapObjectTagSize);
  __ Subu(scratch, scratch, at);
  uint32_t mask2 = kSecondaryTableSize - 1;
  __ Addu(scratch, scratch, Operand((flags >> kHeapObjectTagSize) & mask2));
  __ And(scratch, scratch, Operand(mask2));

  // Probe the secondary table.
  ProbeTable(isolate,
             masm,
             flags,
             kSecondary,
             receiver,
             name,
             scratch,
             extra,
             extra2,
             extra3);

  // Cache miss: Fall-through and let caller handle the miss by
  // entering the runtime system.
  __ bind(&miss);
  __ IncrementCounter(counters->megamorphic_stub_cache_misses(), 1,
                      extra2, extra3);
}


void StubCompiler::GenerateLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                       int index,
                                                       Register prototype) {
  // Load the global or builtins object from the current context.
  __ lw(prototype,
        MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  // Load the native context from the global or builtins object.
  __ lw(prototype,
         FieldMemOperand(prototype, GlobalObject::kNativeContextOffset));
  // Load the function from the native context.
  __ lw(prototype, MemOperand(prototype, Context::SlotOffset(index)));
  // Load the initial map.  The global functions all have initial maps.
  __ lw(prototype,
         FieldMemOperand(prototype, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the prototype from the initial map.
  __ lw(prototype, FieldMemOperand(prototype, Map::kPrototypeOffset));
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
  __ lw(scratch, MemOperand(cp, offset));
  __ lw(scratch, FieldMemOperand(scratch, GlobalObject::kNativeContextOffset));
  __ lw(scratch, MemOperand(scratch, Context::SlotOffset(index)));
  __ li(at, function);
  __ Branch(miss, ne, at, Operand(scratch));

  // Load its initial map. The global functions all have initial maps.
  __ li(prototype, Handle<Map>(function->initial_map()));
  // Load the prototype from the initial map.
  __ lw(prototype, FieldMemOperand(prototype, Map::kPrototypeOffset));
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
    __ lw(dst, FieldMemOperand(src, JSObject::kPropertiesOffset));
    src = dst;
  }
  __ lw(dst, FieldMemOperand(src, offset));
}


void StubCompiler::GenerateLoadArrayLength(MacroAssembler* masm,
                                           Register receiver,
                                           Register scratch,
                                           Label* miss_label) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss_label);

  // Check that the object is a JS array.
  __ GetObjectType(receiver, scratch, scratch);
  __ Branch(miss_label, ne, scratch, Operand(JS_ARRAY_TYPE));

  // Load length directly from the JS array.
  __ Ret(USE_DELAY_SLOT);
  __ lw(v0, FieldMemOperand(receiver, JSArray::kLengthOffset));
}


void StubCompiler::GenerateLoadFunctionPrototype(MacroAssembler* masm,
                                                 Register receiver,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* miss_label) {
  __ TryGetFunctionPrototype(receiver, scratch1, scratch2, miss_label);
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, scratch1);
}


void StubCompiler::GenerateCheckPropertyCell(MacroAssembler* masm,
                                             Handle<JSGlobalObject> global,
                                             Handle<Name> name,
                                             Register scratch,
                                             Label* miss) {
  Handle<Cell> cell = JSGlobalObject::EnsurePropertyCell(global, name);
  ASSERT(cell->value()->IsTheHole());
  __ li(scratch, Operand(cell));
  __ lw(scratch, FieldMemOperand(scratch, Cell::kValueOffset));
  __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
  __ Branch(miss, ne, scratch, Operand(at));
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


// Generate StoreTransition code, value is passed in a0 register.
// After executing generated code, the receiver_reg and name_reg
// may be clobbered.
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
                                                Register scratch3,
                                                Label* miss_label,
                                                Label* slow) {
  // a0 : value.
  Label exit;

  int descriptor = transition->LastAdded();
  DescriptorArray* descriptors = transition->instance_descriptors();
  PropertyDetails details = descriptors->GetDetails(descriptor);
  Representation representation = details.representation();
  ASSERT(!representation.IsNone());

  if (details.type() == CONSTANT) {
    Handle<Object> constant(descriptors->GetValue(descriptor), masm->isolate());
    __ li(scratch1, constant);
    __ Branch(miss_label, ne, value_reg, Operand(scratch1));
  } else if (representation.IsSmi()) {
    __ JumpIfNotSmi(value_reg, miss_label);
  } else if (representation.IsHeapObject()) {
    __ JumpIfSmi(value_reg, miss_label);
  } else if (representation.IsDouble()) {
    Label do_store, heap_number;
    __ LoadRoot(scratch3, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(storage_reg, scratch1, scratch2, scratch3, slow);

    __ JumpIfNotSmi(value_reg, &heap_number);
    __ SmiUntag(scratch1, value_reg);
    __ mtc1(scratch1, f6);
    __ cvt_d_w(f4, f6);
    __ jmp(&do_store);

    __ bind(&heap_number);
    __ CheckMap(value_reg, scratch1, Heap::kHeapNumberMapRootIndex,
                miss_label, DONT_DO_SMI_CHECK);
    __ ldc1(f4, FieldMemOperand(value_reg, HeapNumber::kValueOffset));

    __ bind(&do_store);
    __ sdc1(f4, FieldMemOperand(storage_reg, HeapNumber::kValueOffset));
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  // Perform map transition for the receiver if necessary.
  if (details.type() == FIELD &&
      object->map()->unused_property_fields() == 0) {
    // The properties must be extended before we can store the value.
    // We jump to a runtime call that extends the properties array.
    __ push(receiver_reg);
    __ li(a2, Operand(transition));
    __ Push(a2, a0);
    __ TailCallExternalReference(
           ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage),
                             masm->isolate()),
           3, 1);
    return;
  }

  // Update the map of the object.
  __ li(scratch1, Operand(transition));
  __ sw(scratch1, FieldMemOperand(receiver_reg, HeapObject::kMapOffset));

  // Update the write barrier for the map field.
  __ RecordWriteField(receiver_reg,
                      HeapObject::kMapOffset,
                      scratch1,
                      scratch2,
                      kRAHasNotBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  if (details.type() == CONSTANT) {
    ASSERT(value_reg.is(a0));
    __ Ret(USE_DELAY_SLOT);
    __ mov(v0, a0);
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
      __ sw(storage_reg, FieldMemOperand(receiver_reg, offset));
    } else {
      __ sw(value_reg, FieldMemOperand(receiver_reg, offset));
    }

    if (!representation.IsSmi()) {
      // Update the write barrier for the array address.
      if (!representation.IsDouble()) {
        __ mov(storage_reg, value_reg);
      }
      __ RecordWriteField(receiver_reg,
                          offset,
                          storage_reg,
                          scratch1,
                          kRAHasNotBeenSaved,
                          kDontSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          smi_check);
    }
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array
    __ lw(scratch1,
          FieldMemOperand(receiver_reg, JSObject::kPropertiesOffset));
    if (representation.IsDouble()) {
      __ sw(storage_reg, FieldMemOperand(scratch1, offset));
    } else {
      __ sw(value_reg, FieldMemOperand(scratch1, offset));
    }

    if (!representation.IsSmi()) {
      // Update the write barrier for the array address.
      if (!representation.IsDouble()) {
        __ mov(storage_reg, value_reg);
      }
      __ RecordWriteField(scratch1,
                          offset,
                          storage_reg,
                          receiver_reg,
                          kRAHasNotBeenSaved,
                          kDontSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          smi_check);
    }
  }

  // Return the value (register v0).
  ASSERT(value_reg.is(a0));
  __ bind(&exit);
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, a0);
}


// Generate StoreField code, value is passed in a0 register.
// When leaving generated code after success, the receiver_reg and name_reg
// may be clobbered.  Upon branch to miss_label, the receiver and name
// registers have their original values.
void StoreStubCompiler::GenerateStoreField(MacroAssembler* masm,
                                           Handle<JSObject> object,
                                           LookupResult* lookup,
                                           Register receiver_reg,
                                           Register name_reg,
                                           Register value_reg,
                                           Register scratch1,
                                           Register scratch2,
                                           Label* miss_label) {
  // a0 : value
  Label exit;

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
  } else if (representation.IsDouble()) {
    // Load the double storage.
    if (index < 0) {
      int offset = object->map()->instance_size() + (index * kPointerSize);
      __ lw(scratch1, FieldMemOperand(receiver_reg, offset));
    } else {
      __ lw(scratch1,
            FieldMemOperand(receiver_reg, JSObject::kPropertiesOffset));
      int offset = index * kPointerSize + FixedArray::kHeaderSize;
      __ lw(scratch1, FieldMemOperand(scratch1, offset));
    }

    // Store the value into the storage.
    Label do_store, heap_number;
    __ JumpIfNotSmi(value_reg, &heap_number);
    __ SmiUntag(scratch2, value_reg);
    __ mtc1(scratch2, f6);
    __ cvt_d_w(f4, f6);
    __ jmp(&do_store);

    __ bind(&heap_number);
    __ CheckMap(value_reg, scratch2, Heap::kHeapNumberMapRootIndex,
                miss_label, DONT_DO_SMI_CHECK);
    __ ldc1(f4, FieldMemOperand(value_reg, HeapNumber::kValueOffset));

    __ bind(&do_store);
    __ sdc1(f4, FieldMemOperand(scratch1, HeapNumber::kValueOffset));
    // Return the value (register v0).
    ASSERT(value_reg.is(a0));
    __ Ret(USE_DELAY_SLOT);
    __ mov(v0, a0);
    return;
  }

  // TODO(verwaest): Share this code as a code stub.
  SmiCheck smi_check = representation.IsTagged()
      ? INLINE_SMI_CHECK : OMIT_SMI_CHECK;
  if (index < 0) {
    // Set the property straight into the object.
    int offset = object->map()->instance_size() + (index * kPointerSize);
    __ sw(value_reg, FieldMemOperand(receiver_reg, offset));

    if (!representation.IsSmi()) {
      // Skip updating write barrier if storing a smi.
      __ JumpIfSmi(value_reg, &exit);

      // Update the write barrier for the array address.
      // Pass the now unused name_reg as a scratch register.
      __ mov(name_reg, value_reg);
      __ RecordWriteField(receiver_reg,
                          offset,
                          name_reg,
                          scratch1,
                          kRAHasNotBeenSaved,
                          kDontSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          smi_check);
    }
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array.
    __ lw(scratch1,
          FieldMemOperand(receiver_reg, JSObject::kPropertiesOffset));
    __ sw(value_reg, FieldMemOperand(scratch1, offset));

    if (!representation.IsSmi()) {
      // Skip updating write barrier if storing a smi.
      __ JumpIfSmi(value_reg, &exit);

      // Update the write barrier for the array address.
      // Ok to clobber receiver_reg and name_reg, since we return.
      __ mov(name_reg, value_reg);
      __ RecordWriteField(scratch1,
                          offset,
                          name_reg,
                          receiver_reg,
                          kRAHasNotBeenSaved,
                          kDontSaveFPRegs,
                          EMIT_REMEMBERED_SET,
                          smi_check);
    }
  }

  // Return the value (register v0).
  ASSERT(value_reg.is(a0));
  __ bind(&exit);
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, a0);
}


void StoreStubCompiler::GenerateRestoreName(MacroAssembler* masm,
                                            Label* label,
                                            Handle<Name> name) {
  if (!label->is_unused()) {
    __ bind(label);
    __ li(this->name(), Operand(name));
  }
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
  __ push(name);
  Handle<InterceptorInfo> interceptor(holder_obj->GetNamedInterceptor());
  ASSERT(!masm->isolate()->heap()->InNewSpace(*interceptor));
  Register scratch = name;
  __ li(scratch, Operand(interceptor));
  __ Push(scratch, receiver, holder);
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
  ASSERT(!receiver.is(scratch_in));
  // Preparing to push, adjust sp.
  __ Subu(sp, sp, Operand((argc + 1) * kPointerSize));
  __ sw(receiver, MemOperand(sp, argc * kPointerSize));  // Push receiver.
  // Write the arguments to stack frame.
  for (int i = 0; i < argc; i++) {
    Register arg = values[argc-1-i];
    ASSERT(!receiver.is(arg));
    ASSERT(!scratch_in.is(arg));
    __ sw(arg, MemOperand(sp, (argc-1-i) * kPointerSize));  // Push arg.
  }
  ASSERT(optimization.is_simple_api_call());

  // Abi for CallApiFunctionStub.
  Register callee = a0;
  Register call_data = t0;
  Register holder = a2;
  Register api_function_address = a1;

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
      __ li(holder, api_holder);
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
  __ li(callee, function);

  bool call_data_undefined = false;
  // Put call_data in place.
  if (isolate->heap()->InNewSpace(*call_data_obj)) {
    __ li(call_data, api_call_info);
    __ lw(call_data, FieldMemOperand(call_data, CallHandlerInfo::kDataOffset));
  } else if (call_data_obj->IsUndefined()) {
    call_data_undefined = true;
    __ LoadRoot(call_data, Heap::kUndefinedValueRootIndex);
  } else {
    __ li(call_data, call_data_obj);
  }
  // Put api_function_address in place.
  Address function_address = v8::ToCData<Address>(api_call_info->callback());
  ApiFunction fun(function_address);
  ExternalReference::Type type = ExternalReference::DIRECT_API_CALL;
  ExternalReference ref =
      ExternalReference(&fun,
                        type,
                        masm->isolate());
  __ li(api_function_address, Operand(ref));

  // Jump to stub.
  CallApiFunctionStub stub(is_store, call_data_undefined, argc);
  __ TailCallStub(&stub);
}


void StubCompiler::GenerateTailCall(MacroAssembler* masm, Handle<Code> code) {
  __ Jump(code, RelocInfo::CODE_TARGET);
}


#undef __
#define __ ACCESS_MASM(masm())


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

  // Keep track of the current object in register reg.
  Register reg = object_reg;
  int depth = 0;

  Handle<JSObject> current = Handle<JSObject>::null();
  if (type->IsConstant()) current = Handle<JSObject>::cast(type->AsConstant());
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
             current->property_dictionary()->FindEntry(*name) ==
             NameDictionary::kNotFound);

      GenerateDictionaryNegativeLookup(masm(), miss, reg, name,
                                       scratch1, scratch2);

      __ lw(scratch1, FieldMemOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // From now on the object will be in holder_reg.
      __ lw(reg, FieldMemOperand(scratch1, Map::kPrototypeOffset));
    } else {
      Register map_reg = scratch1;
      if (depth != 1 || check == CHECK_ALL_MAPS) {
        // CheckMap implicitly loads the map of |reg| into |map_reg|.
        __ CheckMap(reg, map_reg, current_map, miss, DONT_DO_SMI_CHECK);
      } else {
        __ lw(map_reg, FieldMemOperand(reg, HeapObject::kMapOffset));
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

      if (heap()->InNewSpace(*prototype)) {
        // The prototype is in new space; we cannot store a reference to it
        // in the code.  Load it from the map.
        __ lw(reg, FieldMemOperand(map_reg, Map::kPrototypeOffset));
      } else {
        // The prototype is in old space; load it directly.
        __ li(reg, Operand(prototype));
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
    __ CheckMap(reg, scratch1, current_map, miss, DONT_DO_SMI_CHECK);
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
    __ Branch(&success);
    __ bind(miss);
    TailCallBuiltin(masm(), MissBuiltin(kind()));
    __ bind(&success);
  }
}


void StoreStubCompiler::HandlerFrontendFooter(Handle<Name> name, Label* miss) {
  if (!miss->is_unused()) {
    Label success;
    __ Branch(&success);
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
    __ lw(dictionary, FieldMemOperand(reg, JSObject::kPropertiesOffset));

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
    // pointer into the dictionary. Check that the value is the callback.
    Register pointer = scratch3();
    const int kElementsStartOffset = NameDictionary::kHeaderSize +
        NameDictionary::kElementsStartIndex * kPointerSize;
    const int kValueOffset = kElementsStartOffset + kPointerSize;
    __ lw(scratch2(), FieldMemOperand(pointer, kValueOffset));
    __ Branch(&miss, ne, scratch2(), Operand(callback));
  }

  HandlerFrontendFooter(name, &miss);
  return reg;
}


void LoadStubCompiler::GenerateLoadField(Register reg,
                                         Handle<JSObject> holder,
                                         PropertyIndex field,
                                         Representation representation) {
  if (!reg.is(receiver())) __ mov(receiver(), reg);
  if (kind() == Code::LOAD_IC) {
    LoadFieldStub stub(field.is_inobject(holder),
                       field.translate(holder),
                       representation);
    GenerateTailCall(masm(), stub.GetCode(isolate()));
  } else {
    KeyedLoadFieldStub stub(field.is_inobject(holder),
                            field.translate(holder),
                            representation);
    GenerateTailCall(masm(), stub.GetCode(isolate()));
  }
}


void LoadStubCompiler::GenerateLoadConstant(Handle<Object> value) {
  // Return the constant value.
  __ li(v0, value);
  __ Ret();
}


void LoadStubCompiler::GenerateLoadCallback(
    Register reg,
    Handle<ExecutableAccessorInfo> callback) {
  // Build AccessorInfo::args_ list on the stack and push property name below
  // the exit frame to make GC aware of them and store pointers to them.
  STATIC_ASSERT(PropertyCallbackArguments::kHolderIndex == 0);
  STATIC_ASSERT(PropertyCallbackArguments::kIsolateIndex == 1);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueDefaultValueIndex == 2);
  STATIC_ASSERT(PropertyCallbackArguments::kReturnValueOffset == 3);
  STATIC_ASSERT(PropertyCallbackArguments::kDataIndex == 4);
  STATIC_ASSERT(PropertyCallbackArguments::kThisIndex == 5);
  STATIC_ASSERT(PropertyCallbackArguments::kArgsLength == 6);
  ASSERT(!scratch2().is(reg));
  ASSERT(!scratch3().is(reg));
  ASSERT(!scratch4().is(reg));
  __ push(receiver());
  if (heap()->InNewSpace(callback->data())) {
    __ li(scratch3(), callback);
    __ lw(scratch3(), FieldMemOperand(scratch3(),
                                      ExecutableAccessorInfo::kDataOffset));
  } else {
    __ li(scratch3(), Handle<Object>(callback->data(), isolate()));
  }
  __ Subu(sp, sp, 6 * kPointerSize);
  __ sw(scratch3(), MemOperand(sp, 5 * kPointerSize));
  __ LoadRoot(scratch3(), Heap::kUndefinedValueRootIndex);
  __ sw(scratch3(), MemOperand(sp, 4 * kPointerSize));
  __ sw(scratch3(), MemOperand(sp, 3 * kPointerSize));
  __ li(scratch4(),
        Operand(ExternalReference::isolate_address(isolate())));
  __ sw(scratch4(), MemOperand(sp, 2 * kPointerSize));
  __ sw(reg, MemOperand(sp, 1 * kPointerSize));
  __ sw(name(), MemOperand(sp, 0 * kPointerSize));
  __ Addu(scratch2(), sp, 1 * kPointerSize);

  __ mov(a2, scratch2());  // Saved in case scratch2 == a1.
  // Abi for CallApiGetter.
  Register getter_address_reg = a2;

  Address getter_address = v8::ToCData<Address>(callback->getter());
  ApiFunction fun(getter_address);
  ExternalReference::Type type = ExternalReference::DIRECT_GETTER_CALL;
  ExternalReference ref = ExternalReference(&fun, type, isolate());
  __ li(getter_address_reg, Operand(ref));

  CallApiGetterStub stub;
  __ TailCallStub(&stub);
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
        __ Push(receiver(), holder_reg, this->name());
      } else {
        __ Push(holder_reg, this->name());
      }
      // Invoke an interceptor.  Note: map checks from receiver to
      // interceptor's holder has been compiled before (see a caller
      // of this method).
      CompileCallLoadPropertyWithInterceptor(
          masm(), receiver(), holder_reg, this->name(), interceptor_holder,
          IC::kLoadPropertyWithInterceptorOnly);

      // Check if interceptor provided a value for property.  If it's
      // the case, return immediately.
      Label interceptor_failed;
      __ LoadRoot(scratch1(), Heap::kNoInterceptorResultSentinelRootIndex);
      __ Branch(&interceptor_failed, eq, v0, Operand(scratch1()));
      frame_scope.GenerateLeaveFrame();
      __ Ret();

      __ bind(&interceptor_failed);
      __ pop(this->name());
      __ pop(holder_reg);
      if (must_preserve_receiver_reg) {
        __ pop(receiver());
      }
      // Leave the internal frame.
    }
    GenerateLoadPostInterceptor(holder_reg, interceptor_holder, name, lookup);
  } else {  // !compile_followup_inline
    // Call the runtime system to load the interceptor.
    // Check that the maps haven't changed.
    PushInterceptorArguments(masm(), receiver(), holder_reg,
                             this->name(), interceptor_holder);

    ExternalReference ref = ExternalReference(
        IC_Utility(IC::kLoadPropertyWithInterceptorForLoad), isolate());
    __ TailCallExternalReference(ref, StubCache::kInterceptorArgsLength, 1);
  }
}


void StubCompiler::GenerateBooleanCheck(Register object, Label* miss) {
  Label success;
  // Check that the object is a boolean.
  __ LoadRoot(at, Heap::kTrueValueRootIndex);
  __ Branch(&success, eq, object, Operand(at));
  __ LoadRoot(at, Heap::kFalseValueRootIndex);
  __ Branch(miss, ne, object, Operand(at));
  __ bind(&success);
}


Handle<Code> StoreStubCompiler::CompileStoreCallback(
    Handle<JSObject> object,
    Handle<JSObject> holder,
    Handle<Name> name,
    Handle<ExecutableAccessorInfo> callback) {
  Register holder_reg = HandlerFrontend(
      IC::CurrentTypeOf(object, isolate()), receiver(), holder, name);

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(holder->IsJSGlobalProxy() || !holder->IsAccessCheckNeeded());

  __ Push(receiver(), holder_reg);  // Receiver.
  __ li(at, Operand(callback));  // Callback info.
  __ push(at);
  __ li(at, Operand(name));
  __ Push(at, value());

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
  //  -- ra    : return address
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save value register, so we can restore it later.
    __ push(value());

    if (!setter.is_null()) {
      // Call the JavaScript setter with receiver and value on the stack.
      if (IC::TypeToMap(*type, masm->isolate())->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ lw(receiver,
               FieldMemOperand(
                   receiver, JSGlobalObject::kGlobalReceiverOffset));
      }
      __ Push(receiver, value());
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
    __ pop(v0);

    // Restore context register.
    __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  }
  __ Ret();
}


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> StoreStubCompiler::CompileStoreInterceptor(
    Handle<JSObject> object,
    Handle<Name> name) {
  __ Push(receiver(), this->name(), value());

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property =
      ExternalReference(IC_Utility(IC::kStoreInterceptorProperty), isolate());
  __ TailCallExternalReference(store_ic_property, 3, 1);

  // Return the generated code.
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> LoadStubCompiler::CompileLoadNonexistent(Handle<HeapType> type,
                                                      Handle<JSObject> last,
                                                      Handle<Name> name) {
  NonexistentHandlerFrontend(type, last, name);

  // Return undefined if maps of the full prototype chain is still the same.
  __ LoadRoot(v0, Heap::kUndefinedValueRootIndex);
  __ Ret();

  // Return the generated code.
  return GetCode(kind(), Code::FAST, name);
}


Register* LoadStubCompiler::registers() {
  // receiver, name, scratch1, scratch2, scratch3, scratch4.
  static Register registers[] = { a0, a2, a3, a1, t0, t1 };
  return registers;
}


Register* KeyedLoadStubCompiler::registers() {
  // receiver, name, scratch1, scratch2, scratch3, scratch4.
  static Register registers[] = { a1, a0, a2, a3, t0, t1 };
  return registers;
}


Register StoreStubCompiler::value() {
  return a0;
}


Register* StoreStubCompiler::registers() {
  // receiver, name, scratch1, scratch2, scratch3.
  static Register registers[] = { a1, a2, a3, t0, t1 };
  return registers;
}


Register* KeyedStoreStubCompiler::registers() {
  // receiver, name, scratch1, scratch2, scratch3.
  static Register registers[] = { a2, a1, a3, t0, t1 };
  return registers;
}


#undef __
#define __ ACCESS_MASM(masm)


void LoadStubCompiler::GenerateLoadViaGetter(MacroAssembler* masm,
                                             Handle<HeapType> type,
                                             Register receiver,
                                             Handle<JSFunction> getter) {
  // ----------- S t a t e -------------
  //  -- a0    : receiver
  //  -- a2    : name
  //  -- ra    : return address
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    if (!getter.is_null()) {
      // Call the JavaScript getter with the receiver on the stack.
      if (IC::TypeToMap(*type, masm->isolate())->IsJSGlobalObjectMap()) {
        // Swap in the global receiver.
        __ lw(receiver,
                FieldMemOperand(
                    receiver, JSGlobalObject::kGlobalReceiverOffset));
      }
      __ push(receiver);
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
    __ lw(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  }
  __ Ret();
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

  HandlerFrontendHeader(type, receiver(), global, name, &miss);

  // Get the value from the cell.
  __ li(a3, Operand(cell));
  __ lw(t0, FieldMemOperand(a3, Cell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (!is_dont_delete) {
    __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
    __ Branch(&miss, eq, t0, Operand(at));
  }

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->named_load_global_stub(), 1, a1, a3);
  __ Ret(USE_DELAY_SLOT);
  __ mov(v0, t0);

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
    __ Branch(&miss, ne, this->name(), Operand(name));
  }

  Label number_case;
  Register match = scratch1();
  Label* smi_target = IncludesNumberType(types) ? &number_case : &miss;
  __ JumpIfSmi(receiver(), smi_target, match);  // Reg match is 0 if Smi.

  Register map_reg = scratch2();

  int receiver_count = types->length();
  int number_of_handled_maps = 0;
  __ lw(map_reg, FieldMemOperand(receiver(), HeapObject::kMapOffset));
  for (int current = 0; current < receiver_count; ++current) {
    Handle<HeapType> type = types->at(current);
    Handle<Map> map = IC::TypeToMap(*type, isolate());
    if (!map->is_deprecated()) {
      number_of_handled_maps++;
      // Check map and tail call if there's a match.
      // Separate compare from branch, to provide path for above JumpIfSmi().
      __ Subu(match, map_reg, Operand(map));
      if (type->Is(HeapType::Number())) {
        ASSERT(!number_case.is_unused());
        __ bind(&number_case);
      }
      __ Jump(handlers->at(current), RelocInfo::CODE_TARGET,
          eq, match, Operand(zero_reg));
    }
  }
  ASSERT(number_of_handled_maps != 0);

  __ bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  InlineCacheState state =
      number_of_handled_maps > 1 ? POLYMORPHIC : MONOMORPHIC;
  return GetICCode(kind(), type, name, state);
}


void StoreStubCompiler::GenerateStoreArrayLength() {
  // Prepare tail call to StoreIC_ArrayLength.
  __ Push(receiver(), value());

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
  __ JumpIfSmi(receiver(), &miss);

  int receiver_count = receiver_maps->length();
  __ lw(scratch1(), FieldMemOperand(receiver(), HeapObject::kMapOffset));
  for (int i = 0; i < receiver_count; ++i) {
    if (transitioned_maps->at(i).is_null()) {
      __ Jump(handler_stubs->at(i), RelocInfo::CODE_TARGET, eq,
          scratch1(), Operand(receiver_maps->at(i)));
    } else {
      Label next_map;
      __ Branch(&next_map, ne, scratch1(), Operand(receiver_maps->at(i)));
      __ li(transition_map(), Operand(transitioned_maps->at(i)));
      __ Jump(handler_stubs->at(i), RelocInfo::CODE_TARGET);
      __ bind(&next_map);
    }
  }

  __ bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  return GetICCode(
      kind(), Code::NORMAL, factory()->empty_string(), POLYMORPHIC);
}


#undef __
#define __ ACCESS_MASM(masm)


void KeyedLoadStubCompiler::GenerateLoadDictionaryElement(
    MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- ra     : return address
  //  -- a0     : key
  //  -- a1     : receiver
  // -----------------------------------
  Label slow, miss;

  Register key = a0;
  Register receiver = a1;

  __ JumpIfNotSmi(key, &miss);
  __ lw(t0, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ sra(a2, a0, kSmiTagSize);
  __ LoadFromNumberDictionary(&slow, t0, a0, v0, a2, a3, t1);
  __ Ret();

  // Slow case, key and receiver still in a0 and a1.
  __ bind(&slow);
  __ IncrementCounter(
      masm->isolate()->counters()->keyed_load_external_array_slow(),
      1, a2, a3);
  // Entry registers are intact.
  // ---------- S t a t e --------------
  //  -- ra     : return address
  //  -- a0     : key
  //  -- a1     : receiver
  // -----------------------------------
  TailCallBuiltin(masm, Builtins::kKeyedLoadIC_Slow);

  // Miss case, call the runtime.
  __ bind(&miss);

  // ---------- S t a t e --------------
  //  -- ra     : return address
  //  -- a0     : key
  //  -- a1     : receiver
  // -----------------------------------
  TailCallBuiltin(masm, Builtins::kKeyedLoadIC_Miss);
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
