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

#if defined(V8_TARGET_ARCH_ARM)

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
  __ add(offset_scratch, offset, Operand(offset, LSL, 1));

  // Calculate the base address of the entry.
  __ mov(base_addr, Operand(key_offset));
  __ add(base_addr, base_addr, Operand(offset_scratch, LSL, kPointerSizeLog2));

  // Check that the key in the entry matches the name.
  __ ldr(ip, MemOperand(base_addr, 0));
  __ cmp(name, ip);
  __ b(ne, &miss);

  // Check the map matches.
  __ ldr(ip, MemOperand(base_addr, map_off_addr - key_off_addr));
  __ ldr(scratch2, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ cmp(ip, scratch2);
  __ b(ne, &miss);

  // Get the code entry from the cache.
  Register code = scratch2;
  scratch2 = no_reg;
  __ ldr(code, MemOperand(base_addr, value_off_addr - key_off_addr));

  // Check that the flags match what we're looking for.
  Register flags_reg = base_addr;
  base_addr = no_reg;
  __ ldr(flags_reg, FieldMemOperand(code, Code::kFlagsOffset));
  // It's a nice optimization if this constant is encodable in the bic insn.

  uint32_t mask = Code::kFlagsNotUsedInLookup;
  ASSERT(__ ImmediateFitsAddrMode1Instruction(mask));
  __ bic(flags_reg, flags_reg, Operand(mask));
  __ cmp(flags_reg, Operand(flags));
  __ b(ne, &miss);

#ifdef DEBUG
    if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
      __ jmp(&miss);
    } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
      __ jmp(&miss);
    }
#endif

  // Jump to the first instruction in the code stub.
  __ add(pc, code, Operand(Code::kHeaderSize - kHeapObjectTag));

  // Miss: fall through.
  __ bind(&miss);
}


// Helper function used to check that the dictionary doesn't contain
// the property. This function may return false negatives, so miss_label
// must always call a backup property check that is complete.
// This function is safe to call if the receiver has fast properties.
// Name must be unique and receiver must be a heap object.
static void GenerateDictionaryNegativeLookup(MacroAssembler* masm,
                                             Label* miss_label,
                                             Register receiver,
                                             Handle<Name> name,
                                             Register scratch0,
                                             Register scratch1) {
  ASSERT(name->IsUniqueName());
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
  __ cmp(scratch0, Operand(FIRST_SPEC_OBJECT_TYPE));
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

  // Check scratch, extra and extra2 registers are valid.
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
  __ ldr(scratch, FieldMemOperand(name, Name::kHashFieldOffset));
  __ ldr(ip, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ add(scratch, scratch, Operand(ip));
  uint32_t mask = kPrimaryTableSize - 1;
  // We shift out the last two bits because they are not part of the hash and
  // they are always 01 for maps.
  __ mov(scratch, Operand(scratch, LSR, kHeapObjectTagSize));
  // Mask down the eor argument to the minimum to keep the immediate
  // ARM-encodable.
  __ eor(scratch, scratch, Operand((flags >> kHeapObjectTagSize) & mask));
  // Prefer and_ to ubfx here because ubfx takes 2 cycles.
  __ and_(scratch, scratch, Operand(mask));

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
  __ sub(scratch, scratch, Operand(name, LSR, kHeapObjectTagSize));
  uint32_t mask2 = kSecondaryTableSize - 1;
  __ add(scratch, scratch, Operand((flags >> kHeapObjectTagSize) & mask2));
  __ and_(scratch, scratch, Operand(mask2));

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
  __ ldr(prototype,
         MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  // Load the native context from the global or builtins object.
  __ ldr(prototype,
         FieldMemOperand(prototype, GlobalObject::kNativeContextOffset));
  // Load the function from the native context.
  __ ldr(prototype, MemOperand(prototype, Context::SlotOffset(index)));
  // Load the initial map.  The global functions all have initial maps.
  __ ldr(prototype,
         FieldMemOperand(prototype, JSFunction::kPrototypeOrInitialMapOffset));
  // Load the prototype from the initial map.
  __ ldr(prototype, FieldMemOperand(prototype, Map::kPrototypeOffset));
}


void StubCompiler::GenerateDirectLoadGlobalFunctionPrototype(
    MacroAssembler* masm,
    int index,
    Register prototype,
    Label* miss) {
  Isolate* isolate = masm->isolate();
  // Check we're still in the same context.
  __ ldr(prototype,
         MemOperand(cp, Context::SlotOffset(Context::GLOBAL_OBJECT_INDEX)));
  __ Move(ip, isolate->global_object());
  __ cmp(prototype, ip);
  __ b(ne, miss);
  // Get the global function with the given index.
  Handle<JSFunction> function(
      JSFunction::cast(isolate->native_context()->get(index)));
  // Load its initial map. The global functions all have initial maps.
  __ Move(prototype, Handle<Map>(function->initial_map()));
  // Load the prototype from the initial map.
  __ ldr(prototype, FieldMemOperand(prototype, Map::kPrototypeOffset));
}


void StubCompiler::GenerateFastPropertyLoad(MacroAssembler* masm,
                                            Register dst,
                                            Register src,
                                            bool inobject,
                                            int index,
                                            Representation representation) {
  ASSERT(!FLAG_track_double_fields || !representation.IsDouble());
  int offset = index * kPointerSize;
  if (!inobject) {
    // Calculate the offset into the properties array.
    offset = offset + FixedArray::kHeaderSize;
    __ ldr(dst, FieldMemOperand(src, JSObject::kPropertiesOffset));
    src = dst;
  }
  __ ldr(dst, FieldMemOperand(src, offset));
}


void StubCompiler::GenerateLoadArrayLength(MacroAssembler* masm,
                                           Register receiver,
                                           Register scratch,
                                           Label* miss_label) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss_label);

  // Check that the object is a JS array.
  __ CompareObjectType(receiver, scratch, scratch, JS_ARRAY_TYPE);
  __ b(ne, miss_label);

  // Load length directly from the JS array.
  __ ldr(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));
  __ Ret();
}


// Generate code to check if an object is a string.  If the object is a
// heap object, its map's instance type is left in the scratch1 register.
// If this is not needed, scratch1 and scratch2 may be the same register.
static void GenerateStringCheck(MacroAssembler* masm,
                                Register receiver,
                                Register scratch1,
                                Register scratch2,
                                Label* smi,
                                Label* non_string_object) {
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, smi);

  // Check that the object is a string.
  __ ldr(scratch1, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ ldrb(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  __ and_(scratch2, scratch1, Operand(kIsNotStringMask));
  // The cast is to resolve the overload for the argument of 0x0.
  __ cmp(scratch2, Operand(static_cast<int32_t>(kStringTag)));
  __ b(ne, non_string_object);
}


// Generate code to load the length from a string object and return the length.
// If the receiver object is not a string or a wrapped string object the
// execution continues at the miss label. The register containing the
// receiver is potentially clobbered.
void StubCompiler::GenerateLoadStringLength(MacroAssembler* masm,
                                            Register receiver,
                                            Register scratch1,
                                            Register scratch2,
                                            Label* miss,
                                            bool support_wrappers) {
  Label check_wrapper;

  // Check if the object is a string leaving the instance type in the
  // scratch1 register.
  GenerateStringCheck(masm, receiver, scratch1, scratch2, miss,
                      support_wrappers ? &check_wrapper : miss);

  // Load length directly from the string.
  __ ldr(r0, FieldMemOperand(receiver, String::kLengthOffset));
  __ Ret();

  if (support_wrappers) {
    // Check if the object is a JSValue wrapper.
    __ bind(&check_wrapper);
    __ cmp(scratch1, Operand(JS_VALUE_TYPE));
    __ b(ne, miss);

    // Unwrap the value and check if the wrapped value is a string.
    __ ldr(scratch1, FieldMemOperand(receiver, JSValue::kValueOffset));
    GenerateStringCheck(masm, scratch1, scratch2, scratch2, miss, miss);
    __ ldr(r0, FieldMemOperand(scratch1, String::kLengthOffset));
    __ Ret();
  }
}


void StubCompiler::GenerateLoadFunctionPrototype(MacroAssembler* masm,
                                                 Register receiver,
                                                 Register scratch1,
                                                 Register scratch2,
                                                 Label* miss_label) {
  __ TryGetFunctionPrototype(receiver, scratch1, scratch2, miss_label);
  __ mov(r0, scratch1);
  __ Ret();
}


// Generate code to check that a global property cell is empty. Create
// the property cell at compilation time if no cell exists for the
// property.
static void GenerateCheckPropertyCell(MacroAssembler* masm,
                                      Handle<GlobalObject> global,
                                      Handle<Name> name,
                                      Register scratch,
                                      Label* miss) {
  Handle<JSGlobalPropertyCell> cell =
      GlobalObject::EnsurePropertyCell(global, name);
  ASSERT(cell->value()->IsTheHole());
  __ mov(scratch, Operand(cell));
  __ ldr(scratch,
         FieldMemOperand(scratch, JSGlobalPropertyCell::kValueOffset));
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ cmp(scratch, ip);
  __ b(ne, miss);
}


// Generate StoreTransition code, value is passed in r0 register.
// When leaving generated code after success, the receiver_reg and name_reg
// may be clobbered.  Upon branch to miss_label, the receiver and name
// registers have their original values.
void StubCompiler::GenerateStoreTransition(MacroAssembler* masm,
                                           Handle<JSObject> object,
                                           LookupResult* lookup,
                                           Handle<Map> transition,
                                           Handle<Name> name,
                                           Register receiver_reg,
                                           Register name_reg,
                                           Register value_reg,
                                           Register scratch1,
                                           Register scratch2,
                                           Register scratch3,
                                           Label* miss_label,
                                           Label* miss_restore_name,
                                           Label* slow) {
  // r0 : value
  Label exit;

  // Check that the map of the object hasn't changed.
  __ CheckMap(receiver_reg, scratch1, Handle<Map>(object->map()), miss_label,
              DO_SMI_CHECK, REQUIRE_EXACT_MAP);

  // Perform global security token check if needed.
  if (object->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(receiver_reg, scratch1, miss_label);
  }

  int descriptor = transition->LastAdded();
  DescriptorArray* descriptors = transition->instance_descriptors();
  PropertyDetails details = descriptors->GetDetails(descriptor);
  Representation representation = details.representation();
  ASSERT(!representation.IsNone());

  // Ensure no transitions to deprecated maps are followed.
  __ CheckMapDeprecated(transition, scratch1, miss_label);

  // Check that we are allowed to write this.
  if (object->GetPrototype()->IsJSObject()) {
    JSObject* holder;
    // holder == object indicates that no property was found.
    if (lookup->holder() != *object) {
      holder = lookup->holder();
    } else {
      // Find the top object.
      holder = *object;
      do {
        holder = JSObject::cast(holder->GetPrototype());
      } while (holder->GetPrototype()->IsJSObject());
    }
    Register holder_reg = CheckPrototypes(
        object, receiver_reg, Handle<JSObject>(holder), name_reg,
        scratch1, scratch2, name, miss_restore_name, SKIP_RECEIVER);
    // If no property was found, and the holder (the last object in the
    // prototype chain) is in slow mode, we need to do a negative lookup on the
    // holder.
    if (lookup->holder() == *object) {
      if (holder->IsJSGlobalObject()) {
        GenerateCheckPropertyCell(
            masm,
            Handle<GlobalObject>(GlobalObject::cast(holder)),
            name,
            scratch1,
            miss_restore_name);
      } else if (!holder->HasFastProperties() && !holder->IsJSGlobalProxy()) {
        GenerateDictionaryNegativeLookup(
            masm, miss_restore_name, holder_reg, name, scratch1, scratch2);
      }
    }
  }

  Register storage_reg = name_reg;

  if (FLAG_track_fields && representation.IsSmi()) {
    __ JumpIfNotSmi(value_reg, miss_restore_name);
  } else if (FLAG_track_double_fields && representation.IsDouble()) {
    Label do_store, heap_number;
    __ LoadRoot(scratch3, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(storage_reg, scratch1, scratch2, scratch3, slow);

    __ JumpIfNotSmi(value_reg, &heap_number);
    __ SmiUntag(scratch1, value_reg);
    __ vmov(s0, scratch1);
    __ vcvt_f64_s32(d0, s0);
    __ jmp(&do_store);

    __ bind(&heap_number);
    __ CheckMap(value_reg, scratch1, Heap::kHeapNumberMapRootIndex,
                miss_restore_name, DONT_DO_SMI_CHECK);
    __ vldr(d0, FieldMemOperand(value_reg, HeapNumber::kValueOffset));

    __ bind(&do_store);
    __ vstr(d0, FieldMemOperand(storage_reg, HeapNumber::kValueOffset));
  }

  // Stub never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  // Perform map transition for the receiver if necessary.
  if (object->map()->unused_property_fields() == 0) {
    // The properties must be extended before we can store the value.
    // We jump to a runtime call that extends the properties array.
    __ push(receiver_reg);
    __ mov(r2, Operand(transition));
    __ Push(r2, r0);
    __ TailCallExternalReference(
        ExternalReference(IC_Utility(IC::kSharedStoreIC_ExtendStorage),
                          masm->isolate()),
        3,
        1);
    return;
  }

  // Update the map of the object.
  __ mov(scratch1, Operand(transition));
  __ str(scratch1, FieldMemOperand(receiver_reg, HeapObject::kMapOffset));

  // Update the write barrier for the map field and pass the now unused
  // name_reg as scratch register.
  __ RecordWriteField(receiver_reg,
                      HeapObject::kMapOffset,
                      scratch1,
                      scratch2,
                      kLRHasNotBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  int index = transition->instance_descriptors()->GetFieldIndex(
      transition->LastAdded());

  // Adjust for the number of properties stored in the object. Even in the
  // face of a transition we can use the old map here because the size of the
  // object and the number of in-object properties is not going to change.
  index -= object->map()->inobject_properties();

  // TODO(verwaest): Share this code as a code stub.
  if (index < 0) {
    // Set the property straight into the object.
    int offset = object->map()->instance_size() + (index * kPointerSize);
    if (FLAG_track_double_fields && representation.IsDouble()) {
      __ str(storage_reg, FieldMemOperand(receiver_reg, offset));
    } else {
      __ str(value_reg, FieldMemOperand(receiver_reg, offset));
    }

    if (!FLAG_track_fields || !representation.IsSmi()) {
      // Skip updating write barrier if storing a smi.
      __ JumpIfSmi(value_reg, &exit);

      // Update the write barrier for the array address.
      // Pass the now unused name_reg as a scratch register.
      if (!FLAG_track_double_fields || !representation.IsDouble()) {
        __ mov(name_reg, value_reg);
      } else {
        ASSERT(storage_reg.is(name_reg));
      }
      __ RecordWriteField(receiver_reg,
                          offset,
                          name_reg,
                          scratch1,
                          kLRHasNotBeenSaved,
                          kDontSaveFPRegs);
    }
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array
    __ ldr(scratch1,
           FieldMemOperand(receiver_reg, JSObject::kPropertiesOffset));
    if (FLAG_track_double_fields && representation.IsDouble()) {
      __ str(storage_reg, FieldMemOperand(scratch1, offset));
    } else {
      __ str(value_reg, FieldMemOperand(scratch1, offset));
    }

    if (!FLAG_track_fields || !representation.IsSmi()) {
      // Skip updating write barrier if storing a smi.
      __ JumpIfSmi(value_reg, &exit);

      // Update the write barrier for the array address.
      // Ok to clobber receiver_reg and name_reg, since we return.
      if (!FLAG_track_double_fields || !representation.IsDouble()) {
        __ mov(name_reg, value_reg);
      } else {
        ASSERT(storage_reg.is(name_reg));
      }
      __ RecordWriteField(scratch1,
                          offset,
                          name_reg,
                          receiver_reg,
                          kLRHasNotBeenSaved,
                          kDontSaveFPRegs);
    }
  }

  // Return the value (register r0).
  ASSERT(value_reg.is(r0));
  __ bind(&exit);
  __ Ret();
}


// Generate StoreField code, value is passed in r0 register.
// When leaving generated code after success, the receiver_reg and name_reg
// may be clobbered.  Upon branch to miss_label, the receiver and name
// registers have their original values.
void StubCompiler::GenerateStoreField(MacroAssembler* masm,
                                      Handle<JSObject> object,
                                      LookupResult* lookup,
                                      Register receiver_reg,
                                      Register name_reg,
                                      Register value_reg,
                                      Register scratch1,
                                      Register scratch2,
                                      Label* miss_label) {
  // r0 : value
  Label exit;

  // Check that the map of the object hasn't changed.
  __ CheckMap(receiver_reg, scratch1, Handle<Map>(object->map()), miss_label,
              DO_SMI_CHECK, ALLOW_ELEMENT_TRANSITION_MAPS);

  // Perform global security token check if needed.
  if (object->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(receiver_reg, scratch1, miss_label);
  }

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
  if (FLAG_track_fields && representation.IsSmi()) {
    __ JumpIfNotSmi(value_reg, miss_label);
  } else if (FLAG_track_double_fields && representation.IsDouble()) {
    // Load the double storage.
    if (index < 0) {
      int offset = object->map()->instance_size() + (index * kPointerSize);
      __ ldr(scratch1, FieldMemOperand(receiver_reg, offset));
    } else {
      __ ldr(scratch1,
             FieldMemOperand(receiver_reg, JSObject::kPropertiesOffset));
      int offset = index * kPointerSize + FixedArray::kHeaderSize;
      __ ldr(scratch1, FieldMemOperand(scratch1, offset));
    }

    // Store the value into the storage.
    Label do_store, heap_number;
    __ JumpIfNotSmi(value_reg, &heap_number);
    __ SmiUntag(scratch2, value_reg);
    __ vmov(s0, scratch2);
    __ vcvt_f64_s32(d0, s0);
    __ jmp(&do_store);

    __ bind(&heap_number);
    __ CheckMap(value_reg, scratch2, Heap::kHeapNumberMapRootIndex,
                miss_label, DONT_DO_SMI_CHECK);
    __ vldr(d0, FieldMemOperand(value_reg, HeapNumber::kValueOffset));

    __ bind(&do_store);
    __ vstr(d0, FieldMemOperand(scratch1, HeapNumber::kValueOffset));
    // Return the value (register r0).
    ASSERT(value_reg.is(r0));
    __ Ret();
    return;
  }

  // TODO(verwaest): Share this code as a code stub.
  if (index < 0) {
    // Set the property straight into the object.
    int offset = object->map()->instance_size() + (index * kPointerSize);
    __ str(value_reg, FieldMemOperand(receiver_reg, offset));

    if (!FLAG_track_fields || !representation.IsSmi()) {
      // Skip updating write barrier if storing a smi.
      __ JumpIfSmi(value_reg, &exit);

      // Update the write barrier for the array address.
      // Pass the now unused name_reg as a scratch register.
      __ mov(name_reg, value_reg);
      __ RecordWriteField(receiver_reg,
                          offset,
                          name_reg,
                          scratch1,
                          kLRHasNotBeenSaved,
                          kDontSaveFPRegs);
    }
  } else {
    // Write to the properties array.
    int offset = index * kPointerSize + FixedArray::kHeaderSize;
    // Get the properties array
    __ ldr(scratch1,
           FieldMemOperand(receiver_reg, JSObject::kPropertiesOffset));
    __ str(value_reg, FieldMemOperand(scratch1, offset));

    if (!FLAG_track_fields || !representation.IsSmi()) {
      // Skip updating write barrier if storing a smi.
      __ JumpIfSmi(value_reg, &exit);

      // Update the write barrier for the array address.
      // Ok to clobber receiver_reg and name_reg, since we return.
      __ mov(name_reg, value_reg);
      __ RecordWriteField(scratch1,
                          offset,
                          name_reg,
                          receiver_reg,
                          kLRHasNotBeenSaved,
                          kDontSaveFPRegs);
    }
  }

  // Return the value (register r0).
  ASSERT(value_reg.is(r0));
  __ bind(&exit);
  __ Ret();
}


void BaseStoreStubCompiler::GenerateRestoreName(MacroAssembler* masm,
                                                Label* label,
                                                Handle<Name> name) {
  if (!label->is_unused()) {
    __ bind(label);
    __ mov(this->name(), Operand(name));
  }
}


static void GenerateCallFunction(MacroAssembler* masm,
                                 Handle<Object> object,
                                 const ParameterCount& arguments,
                                 Label* miss,
                                 Code::ExtraICState extra_ic_state) {
  // ----------- S t a t e -------------
  //  -- r0: receiver
  //  -- r1: function to call
  // -----------------------------------

  // Check that the function really is a function.
  __ JumpIfSmi(r1, miss);
  __ CompareObjectType(r1, r3, r3, JS_FUNCTION_TYPE);
  __ b(ne, miss);

  // Patch the receiver on the stack with the global proxy if
  // necessary.
  if (object->IsGlobalObject()) {
    __ ldr(r3, FieldMemOperand(r0, GlobalObject::kGlobalReceiverOffset));
    __ str(r3, MemOperand(sp, arguments.immediate() * kPointerSize));
  }

  // Invoke the function.
  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  __ InvokeFunction(r1, arguments, JUMP_FUNCTION, NullCallWrapper(), call_kind);
}


static void PushInterceptorArguments(MacroAssembler* masm,
                                     Register receiver,
                                     Register holder,
                                     Register name,
                                     Handle<JSObject> holder_obj) {
  __ push(name);
  Handle<InterceptorInfo> interceptor(holder_obj->GetNamedInterceptor());
  ASSERT(!masm->isolate()->heap()->InNewSpace(*interceptor));
  Register scratch = name;
  __ mov(scratch, Operand(interceptor));
  __ push(scratch);
  __ push(receiver);
  __ push(holder);
  __ ldr(scratch, FieldMemOperand(scratch, InterceptorInfo::kDataOffset));
  __ push(scratch);
  __ mov(scratch, Operand(ExternalReference::isolate_address(masm->isolate())));
  __ push(scratch);
}


static void CompileCallLoadPropertyWithInterceptor(
    MacroAssembler* masm,
    Register receiver,
    Register holder,
    Register name,
    Handle<JSObject> holder_obj) {
  PushInterceptorArguments(masm, receiver, holder, name, holder_obj);

  ExternalReference ref =
      ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorOnly),
                        masm->isolate());
  __ mov(r0, Operand(6));
  __ mov(r1, Operand(ref));

  CEntryStub stub(1);
  __ CallStub(&stub);
}


static const int kFastApiCallArguments = 4;

// Reserves space for the extra arguments to API function in the
// caller's frame.
//
// These arguments are set by CheckPrototypes and GenerateFastApiDirectCall.
static void ReserveSpaceForFastApiCall(MacroAssembler* masm,
                                       Register scratch) {
  __ mov(scratch, Operand(Smi::FromInt(0)));
  for (int i = 0; i < kFastApiCallArguments; i++) {
    __ push(scratch);
  }
}


// Undoes the effects of ReserveSpaceForFastApiCall.
static void FreeSpaceForFastApiCall(MacroAssembler* masm) {
  __ Drop(kFastApiCallArguments);
}


static void GenerateFastApiDirectCall(MacroAssembler* masm,
                                      const CallOptimization& optimization,
                                      int argc) {
  // ----------- S t a t e -------------
  //  -- sp[0]              : holder (set by CheckPrototypes)
  //  -- sp[4]              : callee JS function
  //  -- sp[8]              : call data
  //  -- sp[12]             : isolate
  //  -- sp[16]             : last JS argument
  //  -- ...
  //  -- sp[(argc + 3) * 4] : first JS argument
  //  -- sp[(argc + 4) * 4] : receiver
  // -----------------------------------
  // Get the function and setup the context.
  Handle<JSFunction> function = optimization.constant_function();
  __ LoadHeapObject(r5, function);
  __ ldr(cp, FieldMemOperand(r5, JSFunction::kContextOffset));

  // Pass the additional arguments.
  Handle<CallHandlerInfo> api_call_info = optimization.api_call_info();
  Handle<Object> call_data(api_call_info->data(), masm->isolate());
  if (masm->isolate()->heap()->InNewSpace(*call_data)) {
    __ Move(r0, api_call_info);
    __ ldr(r6, FieldMemOperand(r0, CallHandlerInfo::kDataOffset));
  } else {
    __ Move(r6, call_data);
  }
  __ mov(r7, Operand(ExternalReference::isolate_address(masm->isolate())));
  // Store JS function, call data and isolate.
  __ stm(ib, sp, r5.bit() | r6.bit() | r7.bit());

  // Prepare arguments.
  __ add(r2, sp, Operand(3 * kPointerSize));

  // Allocate the v8::Arguments structure in the arguments' space since
  // it's not controlled by GC.
  const int kApiStackSpace = 4;

  FrameScope frame_scope(masm, StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  // r0 = v8::Arguments&
  // Arguments is after the return address.
  __ add(r0, sp, Operand(1 * kPointerSize));
  // v8::Arguments::implicit_args_
  __ str(r2, MemOperand(r0, 0 * kPointerSize));
  // v8::Arguments::values_
  __ add(ip, r2, Operand(argc * kPointerSize));
  __ str(ip, MemOperand(r0, 1 * kPointerSize));
  // v8::Arguments::length_ = argc
  __ mov(ip, Operand(argc));
  __ str(ip, MemOperand(r0, 2 * kPointerSize));
  // v8::Arguments::is_construct_call = 0
  __ mov(ip, Operand::Zero());
  __ str(ip, MemOperand(r0, 3 * kPointerSize));

  const int kStackUnwindSpace = argc + kFastApiCallArguments + 1;
  Address function_address = v8::ToCData<Address>(api_call_info->callback());
  ApiFunction fun(function_address);
  ExternalReference ref = ExternalReference(&fun,
                                            ExternalReference::DIRECT_API_CALL,
                                            masm->isolate());
  AllowExternalCallThatCantCauseGC scope(masm);

  __ CallApiFunctionAndReturn(ref, kStackUnwindSpace);
}


class CallInterceptorCompiler BASE_EMBEDDED {
 public:
  CallInterceptorCompiler(StubCompiler* stub_compiler,
                          const ParameterCount& arguments,
                          Register name,
                          Code::ExtraICState extra_ic_state)
      : stub_compiler_(stub_compiler),
        arguments_(arguments),
        name_(name),
        extra_ic_state_(extra_ic_state) {}

  void Compile(MacroAssembler* masm,
               Handle<JSObject> object,
               Handle<JSObject> holder,
               Handle<Name> name,
               LookupResult* lookup,
               Register receiver,
               Register scratch1,
               Register scratch2,
               Register scratch3,
               Label* miss) {
    ASSERT(holder->HasNamedInterceptor());
    ASSERT(!holder->GetNamedInterceptor()->getter()->IsUndefined());

    // Check that the receiver isn't a smi.
    __ JumpIfSmi(receiver, miss);
    CallOptimization optimization(lookup);
    if (optimization.is_constant_call()) {
      CompileCacheable(masm, object, receiver, scratch1, scratch2, scratch3,
                       holder, lookup, name, optimization, miss);
    } else {
      CompileRegular(masm, object, receiver, scratch1, scratch2, scratch3,
                     name, holder, miss);
    }
  }

 private:
  void CompileCacheable(MacroAssembler* masm,
                        Handle<JSObject> object,
                        Register receiver,
                        Register scratch1,
                        Register scratch2,
                        Register scratch3,
                        Handle<JSObject> interceptor_holder,
                        LookupResult* lookup,
                        Handle<Name> name,
                        const CallOptimization& optimization,
                        Label* miss_label) {
    ASSERT(optimization.is_constant_call());
    ASSERT(!lookup->holder()->IsGlobalObject());
    Counters* counters = masm->isolate()->counters();
    int depth1 = kInvalidProtoDepth;
    int depth2 = kInvalidProtoDepth;
    bool can_do_fast_api_call = false;
    if (optimization.is_simple_api_call() &&
        !lookup->holder()->IsGlobalObject()) {
      depth1 = optimization.GetPrototypeDepthOfExpectedType(
          object, interceptor_holder);
      if (depth1 == kInvalidProtoDepth) {
        depth2 = optimization.GetPrototypeDepthOfExpectedType(
            interceptor_holder, Handle<JSObject>(lookup->holder()));
      }
      can_do_fast_api_call =
          depth1 != kInvalidProtoDepth || depth2 != kInvalidProtoDepth;
    }

    __ IncrementCounter(counters->call_const_interceptor(), 1,
                        scratch1, scratch2);

    if (can_do_fast_api_call) {
      __ IncrementCounter(counters->call_const_interceptor_fast_api(), 1,
                          scratch1, scratch2);
      ReserveSpaceForFastApiCall(masm, scratch1);
    }

    // Check that the maps from receiver to interceptor's holder
    // haven't changed and thus we can invoke interceptor.
    Label miss_cleanup;
    Label* miss = can_do_fast_api_call ? &miss_cleanup : miss_label;
    Register holder =
        stub_compiler_->CheckPrototypes(object, receiver, interceptor_holder,
                                        scratch1, scratch2, scratch3,
                                        name, depth1, miss);

    // Invoke an interceptor and if it provides a value,
    // branch to |regular_invoke|.
    Label regular_invoke;
    LoadWithInterceptor(masm, receiver, holder, interceptor_holder, scratch2,
                        &regular_invoke);

    // Interceptor returned nothing for this property.  Try to use cached
    // constant function.

    // Check that the maps from interceptor's holder to constant function's
    // holder haven't changed and thus we can use cached constant function.
    if (*interceptor_holder != lookup->holder()) {
      stub_compiler_->CheckPrototypes(interceptor_holder, receiver,
                                      Handle<JSObject>(lookup->holder()),
                                      scratch1, scratch2, scratch3,
                                      name, depth2, miss);
    } else {
      // CheckPrototypes has a side effect of fetching a 'holder'
      // for API (object which is instanceof for the signature).  It's
      // safe to omit it here, as if present, it should be fetched
      // by the previous CheckPrototypes.
      ASSERT(depth2 == kInvalidProtoDepth);
    }

    // Invoke function.
    if (can_do_fast_api_call) {
      GenerateFastApiDirectCall(masm, optimization, arguments_.immediate());
    } else {
      CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state_)
          ? CALL_AS_FUNCTION
          : CALL_AS_METHOD;
      Handle<JSFunction> function = optimization.constant_function();
      ParameterCount expected(function);
      __ InvokeFunction(function, expected, arguments_,
                        JUMP_FUNCTION, NullCallWrapper(), call_kind);
    }

    // Deferred code for fast API call case---clean preallocated space.
    if (can_do_fast_api_call) {
      __ bind(&miss_cleanup);
      FreeSpaceForFastApiCall(masm);
      __ b(miss_label);
    }

    // Invoke a regular function.
    __ bind(&regular_invoke);
    if (can_do_fast_api_call) {
      FreeSpaceForFastApiCall(masm);
    }
  }

  void CompileRegular(MacroAssembler* masm,
                      Handle<JSObject> object,
                      Register receiver,
                      Register scratch1,
                      Register scratch2,
                      Register scratch3,
                      Handle<Name> name,
                      Handle<JSObject> interceptor_holder,
                      Label* miss_label) {
    Register holder =
        stub_compiler_->CheckPrototypes(object, receiver, interceptor_holder,
                                        scratch1, scratch2, scratch3,
                                        name, miss_label);

    // Call a runtime function to load the interceptor property.
    FrameScope scope(masm, StackFrame::INTERNAL);
    // Save the name_ register across the call.
    __ push(name_);
    PushInterceptorArguments(masm, receiver, holder, name_, interceptor_holder);
    __ CallExternalReference(
        ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorForCall),
                          masm->isolate()),
        6);
    // Restore the name_ register.
    __ pop(name_);
    // Leave the internal frame.
  }

  void LoadWithInterceptor(MacroAssembler* masm,
                           Register receiver,
                           Register holder,
                           Handle<JSObject> holder_obj,
                           Register scratch,
                           Label* interceptor_succeeded) {
    {
      FrameScope scope(masm, StackFrame::INTERNAL);
      __ Push(holder, name_);
      CompileCallLoadPropertyWithInterceptor(masm,
                                             receiver,
                                             holder,
                                             name_,
                                             holder_obj);
      __ pop(name_);  // Restore the name.
      __ pop(receiver);  // Restore the holder.
    }
    // If interceptor returns no-result sentinel, call the constant function.
    __ LoadRoot(scratch, Heap::kNoInterceptorResultSentinelRootIndex);
    __ cmp(r0, scratch);
    __ b(ne, interceptor_succeeded);
  }

  StubCompiler* stub_compiler_;
  const ParameterCount& arguments_;
  Register name_;
  Code::ExtraICState extra_ic_state_;
};


// Calls GenerateCheckPropertyCell for each global object in the prototype chain
// from object to (but not including) holder.
static void GenerateCheckPropertyCells(MacroAssembler* masm,
                                       Handle<JSObject> object,
                                       Handle<JSObject> holder,
                                       Handle<Name> name,
                                       Register scratch,
                                       Label* miss) {
  Handle<JSObject> current = object;
  while (!current.is_identical_to(holder)) {
    if (current->IsGlobalObject()) {
      GenerateCheckPropertyCell(masm,
                                Handle<GlobalObject>::cast(current),
                                name,
                                scratch,
                                miss);
    }
    current = Handle<JSObject>(JSObject::cast(current->GetPrototype()));
  }
}


// Convert and store int passed in register ival to IEEE 754 single precision
// floating point value at memory location (dst + 4 * wordoffset)
// If VFP3 is available use it for conversion.
static void StoreIntAsFloat(MacroAssembler* masm,
                            Register dst,
                            Register wordoffset,
                            Register ival,
                            Register scratch1) {
  __ vmov(s0, ival);
  __ add(scratch1, dst, Operand(wordoffset, LSL, 2));
  __ vcvt_f32_s32(s0, s0);
  __ vstr(s0, scratch1, 0);
}


void StubCompiler::GenerateTailCall(MacroAssembler* masm, Handle<Code> code) {
  __ Jump(code, RelocInfo::CODE_TARGET);
}


#undef __
#define __ ACCESS_MASM(masm())


Register StubCompiler::CheckPrototypes(Handle<JSObject> object,
                                       Register object_reg,
                                       Handle<JSObject> holder,
                                       Register holder_reg,
                                       Register scratch1,
                                       Register scratch2,
                                       Handle<Name> name,
                                       int save_at_depth,
                                       Label* miss,
                                       PrototypeCheckType check) {
  Handle<JSObject> first = object;
  // Make sure there's no overlap between holder and object registers.
  ASSERT(!scratch1.is(object_reg) && !scratch1.is(holder_reg));
  ASSERT(!scratch2.is(object_reg) && !scratch2.is(holder_reg)
         && !scratch2.is(scratch1));

  // Keep track of the current object in register reg.
  Register reg = object_reg;
  int depth = 0;

  if (save_at_depth == depth) {
    __ str(reg, MemOperand(sp));
  }

  // Check the maps in the prototype chain.
  // Traverse the prototype chain from the object and do map checks.
  Handle<JSObject> current = object;
  while (!current.is_identical_to(holder)) {
    ++depth;

    // Only global objects and objects that do not require access
    // checks are allowed in stubs.
    ASSERT(current->IsJSGlobalProxy() || !current->IsAccessCheckNeeded());

    Handle<JSObject> prototype(JSObject::cast(current->GetPrototype()));
    if (!current->HasFastProperties() &&
        !current->IsJSGlobalObject() &&
        !current->IsJSGlobalProxy()) {
      if (!name->IsUniqueName()) {
        ASSERT(name->IsString());
        name = factory()->InternalizeString(Handle<String>::cast(name));
      }
      ASSERT(current->property_dictionary()->FindEntry(*name) ==
             NameDictionary::kNotFound);

      GenerateDictionaryNegativeLookup(masm(), miss, reg, name,
                                       scratch1, scratch2);

      __ ldr(scratch1, FieldMemOperand(reg, HeapObject::kMapOffset));
      reg = holder_reg;  // From now on the object will be in holder_reg.
      __ ldr(reg, FieldMemOperand(scratch1, Map::kPrototypeOffset));
    } else {
      Register map_reg = scratch1;
      if (!current.is_identical_to(first) || check == CHECK_ALL_MAPS) {
        Handle<Map> current_map(current->map());
        // CheckMap implicitly loads the map of |reg| into |map_reg|.
        __ CheckMap(reg, map_reg, current_map, miss, DONT_DO_SMI_CHECK,
                    ALLOW_ELEMENT_TRANSITION_MAPS);
      } else {
        __ ldr(map_reg, FieldMemOperand(reg, HeapObject::kMapOffset));
      }

      // Check access rights to the global object.  This has to happen after
      // the map check so that we know that the object is actually a global
      // object.
      if (current->IsJSGlobalProxy()) {
        __ CheckAccessGlobalProxy(reg, scratch2, miss);
      }
      reg = holder_reg;  // From now on the object will be in holder_reg.

      if (heap()->InNewSpace(*prototype)) {
        // The prototype is in new space; we cannot store a reference to it
        // in the code.  Load it from the map.
        __ ldr(reg, FieldMemOperand(map_reg, Map::kPrototypeOffset));
      } else {
        // The prototype is in old space; load it directly.
        __ mov(reg, Operand(prototype));
      }
    }

    if (save_at_depth == depth) {
      __ str(reg, MemOperand(sp));
    }

    // Go to the next object in the prototype chain.
    current = prototype;
  }

  // Log the check depth.
  LOG(isolate(), IntEvent("check-maps-depth", depth + 1));

  if (!holder.is_identical_to(first) || check == CHECK_ALL_MAPS) {
    // Check the holder map.
    __ CheckMap(reg, scratch1, Handle<Map>(holder->map()), miss,
                DONT_DO_SMI_CHECK, ALLOW_ELEMENT_TRANSITION_MAPS);
  }

  // Perform security check for access to the global object.
  ASSERT(holder->IsJSGlobalProxy() || !holder->IsAccessCheckNeeded());
  if (holder->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(reg, scratch1, miss);
  }

  // If we've skipped any global objects, it's not enough to verify that
  // their maps haven't changed.  We also need to check that the property
  // cell for the property is still empty.
  GenerateCheckPropertyCells(masm(), object, holder, name, scratch1, miss);

  // Return the register containing the holder.
  return reg;
}


void BaseLoadStubCompiler::HandlerFrontendFooter(Label* success,
                                                 Label* miss) {
  if (!miss->is_unused()) {
    __ b(success);
    __ bind(miss);
    TailCallBuiltin(masm(), MissBuiltin(kind()));
  }
}


Register BaseLoadStubCompiler::CallbackHandlerFrontend(
    Handle<JSObject> object,
    Register object_reg,
    Handle<JSObject> holder,
    Handle<Name> name,
    Label* success,
    Handle<ExecutableAccessorInfo> callback) {
  Label miss;

  Register reg = HandlerFrontendHeader(object, object_reg, holder, name, &miss);

  if (!holder->HasFastProperties() && !holder->IsJSGlobalObject()) {
    ASSERT(!reg.is(scratch2()));
    ASSERT(!reg.is(scratch3()));
    ASSERT(!reg.is(scratch4()));

    // Load the properties dictionary.
    Register dictionary = scratch4();
    __ ldr(dictionary, FieldMemOperand(reg, JSObject::kPropertiesOffset));

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
    __ ldr(scratch2(), FieldMemOperand(pointer, kValueOffset));
    __ cmp(scratch2(), Operand(callback));
    __ b(ne, &miss);
  }

  HandlerFrontendFooter(success, &miss);
  return reg;
}


void BaseLoadStubCompiler::NonexistentHandlerFrontend(
    Handle<JSObject> object,
    Handle<JSObject> last,
    Handle<Name> name,
    Label* success,
    Handle<GlobalObject> global) {
  Label miss;

  HandlerFrontendHeader(object, receiver(), last, name, &miss);

  // If the last object in the prototype chain is a global object,
  // check that the global property cell is empty.
  if (!global.is_null()) {
    GenerateCheckPropertyCell(masm(), global, name, scratch2(), &miss);
  }

  HandlerFrontendFooter(success, &miss);
}


void BaseLoadStubCompiler::GenerateLoadField(Register reg,
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


void BaseLoadStubCompiler::GenerateLoadConstant(Handle<JSFunction> value) {
  // Return the constant value.
  __ LoadHeapObject(r0, value);
  __ Ret();
}


void BaseLoadStubCompiler::GenerateLoadCallback(
    Register reg,
    Handle<ExecutableAccessorInfo> callback) {
  // Build AccessorInfo::args_ list on the stack and push property name below
  // the exit frame to make GC aware of them and store pointers to them.
  __ push(receiver());
  __ mov(scratch2(), sp);  // scratch2 = AccessorInfo::args_
  if (heap()->InNewSpace(callback->data())) {
    __ Move(scratch3(), callback);
    __ ldr(scratch3(), FieldMemOperand(scratch3(),
                                       ExecutableAccessorInfo::kDataOffset));
  } else {
    __ Move(scratch3(), Handle<Object>(callback->data(), isolate()));
  }
  __ Push(reg, scratch3());
  __ mov(scratch3(),
         Operand(ExternalReference::isolate_address(isolate())));
  __ Push(scratch3(), name());
  __ mov(r0, sp);  // r0 = Handle<Name>

  const int kApiStackSpace = 1;
  FrameScope frame_scope(masm(), StackFrame::MANUAL);
  __ EnterExitFrame(false, kApiStackSpace);

  // Create AccessorInfo instance on the stack above the exit frame with
  // scratch2 (internal::Object** args_) as the data.
  __ str(scratch2(), MemOperand(sp, 1 * kPointerSize));
  __ add(r1, sp, Operand(1 * kPointerSize));  // r1 = AccessorInfo&

  const int kStackUnwindSpace = 5;
  Address getter_address = v8::ToCData<Address>(callback->getter());
  ApiFunction fun(getter_address);
  ExternalReference ref = ExternalReference(
      &fun, ExternalReference::DIRECT_GETTER_CALL, isolate());
  __ CallApiFunctionAndReturn(ref, kStackUnwindSpace);
}


void BaseLoadStubCompiler::GenerateLoadInterceptor(
    Register holder_reg,
    Handle<JSObject> object,
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
      // of this method.)
      CompileCallLoadPropertyWithInterceptor(masm(),
                                             receiver(),
                                             holder_reg,
                                             this->name(),
                                             interceptor_holder);
      // Check if interceptor provided a value for property.  If it's
      // the case, return immediately.
      Label interceptor_failed;
      __ LoadRoot(scratch1(), Heap::kNoInterceptorResultSentinelRootIndex);
      __ cmp(r0, scratch1());
      __ b(eq, &interceptor_failed);
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

    ExternalReference ref =
        ExternalReference(IC_Utility(IC::kLoadPropertyWithInterceptorForLoad),
                          isolate());
    __ TailCallExternalReference(ref, 6, 1);
  }
}


void CallStubCompiler::GenerateNameCheck(Handle<Name> name, Label* miss) {
  if (kind_ == Code::KEYED_CALL_IC) {
    __ cmp(r2, Operand(name));
    __ b(ne, miss);
  }
}


void CallStubCompiler::GenerateGlobalReceiverCheck(Handle<JSObject> object,
                                                   Handle<JSObject> holder,
                                                   Handle<Name> name,
                                                   Label* miss) {
  ASSERT(holder->IsGlobalObject());

  // Get the number of arguments.
  const int argc = arguments().immediate();

  // Get the receiver from the stack.
  __ ldr(r0, MemOperand(sp, argc * kPointerSize));

  // Check that the maps haven't changed.
  __ JumpIfSmi(r0, miss);
  CheckPrototypes(object, r0, holder, r3, r1, r4, name, miss);
}


void CallStubCompiler::GenerateLoadFunctionFromCell(
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Label* miss) {
  // Get the value from the cell.
  __ mov(r3, Operand(cell));
  __ ldr(r1, FieldMemOperand(r3, JSGlobalPropertyCell::kValueOffset));

  // Check that the cell contains the same function.
  if (heap()->InNewSpace(*function)) {
    // We can't embed a pointer to a function in new space so we have
    // to verify that the shared function info is unchanged. This has
    // the nice side effect that multiple closures based on the same
    // function can all use this call IC. Before we load through the
    // function, we have to verify that it still is a function.
    __ JumpIfSmi(r1, miss);
    __ CompareObjectType(r1, r3, r3, JS_FUNCTION_TYPE);
    __ b(ne, miss);

    // Check the shared function info. Make sure it hasn't changed.
    __ Move(r3, Handle<SharedFunctionInfo>(function->shared()));
    __ ldr(r4, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
    __ cmp(r4, r3);
  } else {
    __ cmp(r1, Operand(function));
  }
  __ b(ne, miss);
}


void CallStubCompiler::GenerateMissBranch() {
  Handle<Code> code =
      isolate()->stub_cache()->ComputeCallMiss(arguments().immediate(),
                                               kind_,
                                               extra_state_);
  __ Jump(code, RelocInfo::CODE_TARGET);
}


Handle<Code> CallStubCompiler::CompileCallField(Handle<JSObject> object,
                                                Handle<JSObject> holder,
                                                PropertyIndex index,
                                                Handle<Name> name) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  GenerateNameCheck(name, &miss);

  const int argc = arguments().immediate();

  // Get the receiver of the function from the stack into r0.
  __ ldr(r0, MemOperand(sp, argc * kPointerSize));
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(r0, &miss);

  // Do the right check and compute the holder register.
  Register reg = CheckPrototypes(object, r0, holder, r1, r3, r4, name, &miss);
  GenerateFastPropertyLoad(masm(), r1, reg, index.is_inobject(holder),
                           index.translate(holder), Representation::Tagged());

  GenerateCallFunction(masm(), object, arguments(), &miss, extra_state_);

  // Handle call cache miss.
  __ bind(&miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(Code::FIELD, name);
}


Handle<Code> CallStubCompiler::CompileArrayPushCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || !cell.is_null()) return Handle<Code>::null();

  Label miss;
  GenerateNameCheck(name, &miss);

  Register receiver = r1;
  // Get the receiver from the stack
  const int argc = arguments().immediate();
  __ ldr(receiver, MemOperand(sp, argc * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Check that the maps haven't changed.
  CheckPrototypes(Handle<JSObject>::cast(object), receiver, holder, r3, r0, r4,
                  name, &miss);

  if (argc == 0) {
    // Nothing to do, just return the length.
    __ ldr(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));
    __ Drop(argc + 1);
    __ Ret();
  } else {
    Label call_builtin;

    if (argc == 1) {  // Otherwise fall through to call the builtin.
      Label attempt_to_grow_elements, with_write_barrier, check_double;

      Register elements = r6;
      Register end_elements = r5;
      // Get the elements array of the object.
      __ ldr(elements, FieldMemOperand(receiver, JSArray::kElementsOffset));

      // Check that the elements are in fast mode and writable.
      __ CheckMap(elements,
                  r0,
                  Heap::kFixedArrayMapRootIndex,
                  &check_double,
                  DONT_DO_SMI_CHECK);

      // Get the array's length into r0 and calculate new length.
      __ ldr(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));
      STATIC_ASSERT(kSmiTagSize == 1);
      STATIC_ASSERT(kSmiTag == 0);
      __ add(r0, r0, Operand(Smi::FromInt(argc)));

      // Get the elements' length.
      __ ldr(r4, FieldMemOperand(elements, FixedArray::kLengthOffset));

      // Check if we could survive without allocation.
      __ cmp(r0, r4);
      __ b(gt, &attempt_to_grow_elements);

      // Check if value is a smi.
      __ ldr(r4, MemOperand(sp, (argc - 1) * kPointerSize));
      __ JumpIfNotSmi(r4, &with_write_barrier);

      // Save new length.
      __ str(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));

      // Store the value.
      // We may need a register containing the address end_elements below,
      // so write back the value in end_elements.
      __ add(end_elements, elements,
             Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));
      const int kEndElementsOffset =
          FixedArray::kHeaderSize - kHeapObjectTag - argc * kPointerSize;
      __ str(r4, MemOperand(end_elements, kEndElementsOffset, PreIndex));

      // Check for a smi.
      __ Drop(argc + 1);
      __ Ret();

      __ bind(&check_double);

      // Check that the elements are in fast mode and writable.
      __ CheckMap(elements,
                  r0,
                  Heap::kFixedDoubleArrayMapRootIndex,
                  &call_builtin,
                  DONT_DO_SMI_CHECK);

      // Get the array's length into r0 and calculate new length.
      __ ldr(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));
      STATIC_ASSERT(kSmiTagSize == 1);
      STATIC_ASSERT(kSmiTag == 0);
      __ add(r0, r0, Operand(Smi::FromInt(argc)));

      // Get the elements' length.
      __ ldr(r4, FieldMemOperand(elements, FixedArray::kLengthOffset));

      // Check if we could survive without allocation.
      __ cmp(r0, r4);
      __ b(gt, &call_builtin);

      __ ldr(r4, MemOperand(sp, (argc - 1) * kPointerSize));
      __ StoreNumberToDoubleElements(r4, r0, elements, r5,
                                     &call_builtin, argc * kDoubleSize);

      // Save new length.
      __ str(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));

      // Check for a smi.
      __ Drop(argc + 1);
      __ Ret();

      __ bind(&with_write_barrier);

      __ ldr(r3, FieldMemOperand(receiver, HeapObject::kMapOffset));

      if (FLAG_smi_only_arrays  && !FLAG_trace_elements_transitions) {
        Label fast_object, not_fast_object;
        __ CheckFastObjectElements(r3, r7, &not_fast_object);
        __ jmp(&fast_object);
        // In case of fast smi-only, convert to fast object, otherwise bail out.
        __ bind(&not_fast_object);
        __ CheckFastSmiElements(r3, r7, &call_builtin);

        __ ldr(r7, FieldMemOperand(r4, HeapObject::kMapOffset));
        __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
        __ cmp(r7, ip);
        __ b(eq, &call_builtin);
        // edx: receiver
        // r3: map
        Label try_holey_map;
        __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                               FAST_ELEMENTS,
                                               r3,
                                               r7,
                                               &try_holey_map);
        __ mov(r2, receiver);
        ElementsTransitionGenerator::
            GenerateMapChangeElementsTransition(masm(),
                                                DONT_TRACK_ALLOCATION_SITE,
                                                NULL);
        __ jmp(&fast_object);

        __ bind(&try_holey_map);
        __ LoadTransitionedArrayMapConditional(FAST_HOLEY_SMI_ELEMENTS,
                                               FAST_HOLEY_ELEMENTS,
                                               r3,
                                               r7,
                                               &call_builtin);
        __ mov(r2, receiver);
        ElementsTransitionGenerator::
            GenerateMapChangeElementsTransition(masm(),
                                                DONT_TRACK_ALLOCATION_SITE,
                                                NULL);
        __ bind(&fast_object);
      } else {
        __ CheckFastObjectElements(r3, r3, &call_builtin);
      }

      // Save new length.
      __ str(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));

      // Store the value.
      // We may need a register containing the address end_elements below,
      // so write back the value in end_elements.
      __ add(end_elements, elements,
             Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));
      __ str(r4, MemOperand(end_elements, kEndElementsOffset, PreIndex));

      __ RecordWrite(elements,
                     end_elements,
                     r4,
                     kLRHasNotBeenSaved,
                     kDontSaveFPRegs,
                     EMIT_REMEMBERED_SET,
                     OMIT_SMI_CHECK);
      __ Drop(argc + 1);
      __ Ret();

      __ bind(&attempt_to_grow_elements);
      // r0: array's length + 1.
      // r4: elements' length.

      if (!FLAG_inline_new) {
        __ b(&call_builtin);
      }

      __ ldr(r2, MemOperand(sp, (argc - 1) * kPointerSize));
      // Growing elements that are SMI-only requires special handling in case
      // the new element is non-Smi. For now, delegate to the builtin.
      Label no_fast_elements_check;
      __ JumpIfSmi(r2, &no_fast_elements_check);
      __ ldr(r7, FieldMemOperand(receiver, HeapObject::kMapOffset));
      __ CheckFastObjectElements(r7, r7, &call_builtin);
      __ bind(&no_fast_elements_check);

      ExternalReference new_space_allocation_top =
          ExternalReference::new_space_allocation_top_address(isolate());
      ExternalReference new_space_allocation_limit =
          ExternalReference::new_space_allocation_limit_address(isolate());

      const int kAllocationDelta = 4;
      // Load top and check if it is the end of elements.
      __ add(end_elements, elements,
             Operand(r0, LSL, kPointerSizeLog2 - kSmiTagSize));
      __ add(end_elements, end_elements, Operand(kEndElementsOffset));
      __ mov(r7, Operand(new_space_allocation_top));
      __ ldr(r3, MemOperand(r7));
      __ cmp(end_elements, r3);
      __ b(ne, &call_builtin);

      __ mov(r9, Operand(new_space_allocation_limit));
      __ ldr(r9, MemOperand(r9));
      __ add(r3, r3, Operand(kAllocationDelta * kPointerSize));
      __ cmp(r3, r9);
      __ b(hi, &call_builtin);

      // We fit and could grow elements.
      // Update new_space_allocation_top.
      __ str(r3, MemOperand(r7));
      // Push the argument.
      __ str(r2, MemOperand(end_elements));
      // Fill the rest with holes.
      __ LoadRoot(r3, Heap::kTheHoleValueRootIndex);
      for (int i = 1; i < kAllocationDelta; i++) {
        __ str(r3, MemOperand(end_elements, i * kPointerSize));
      }

      // Update elements' and array's sizes.
      __ str(r0, FieldMemOperand(receiver, JSArray::kLengthOffset));
      __ add(r4, r4, Operand(Smi::FromInt(kAllocationDelta)));
      __ str(r4, FieldMemOperand(elements, FixedArray::kLengthOffset));

      // Elements are in new space, so write barrier is not required.
      __ Drop(argc + 1);
      __ Ret();
    }
    __ bind(&call_builtin);
    __ TailCallExternalReference(
        ExternalReference(Builtins::c_ArrayPush, isolate()), argc + 1, 1);
  }

  // Handle call cache miss.
  __ bind(&miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(function);
}


Handle<Code> CallStubCompiler::CompileArrayPopCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // If object is not an array, bail out to regular call.
  if (!object->IsJSArray() || !cell.is_null()) return Handle<Code>::null();

  Label miss, return_undefined, call_builtin;
  Register receiver = r1;
  Register elements = r3;
  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack
  const int argc = arguments().immediate();
  __ ldr(receiver, MemOperand(sp, argc * kPointerSize));
  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Check that the maps haven't changed.
  CheckPrototypes(Handle<JSObject>::cast(object), receiver, holder, elements,
                  r4, r0, name, &miss);

  // Get the elements array of the object.
  __ ldr(elements, FieldMemOperand(receiver, JSArray::kElementsOffset));

  // Check that the elements are in fast mode and writable.
  __ CheckMap(elements,
              r0,
              Heap::kFixedArrayMapRootIndex,
              &call_builtin,
              DONT_DO_SMI_CHECK);

  // Get the array's length into r4 and calculate new length.
  __ ldr(r4, FieldMemOperand(receiver, JSArray::kLengthOffset));
  __ sub(r4, r4, Operand(Smi::FromInt(1)), SetCC);
  __ b(lt, &return_undefined);

  // Get the last element.
  __ LoadRoot(r6, Heap::kTheHoleValueRootIndex);
  STATIC_ASSERT(kSmiTagSize == 1);
  STATIC_ASSERT(kSmiTag == 0);
  // We can't address the last element in one operation. Compute the more
  // expensive shift first, and use an offset later on.
  __ add(elements, elements, Operand(r4, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ ldr(r0, FieldMemOperand(elements, FixedArray::kHeaderSize));
  __ cmp(r0, r6);
  __ b(eq, &call_builtin);

  // Set the array's length.
  __ str(r4, FieldMemOperand(receiver, JSArray::kLengthOffset));

  // Fill with the hole.
  __ str(r6, FieldMemOperand(elements, FixedArray::kHeaderSize));
  __ Drop(argc + 1);
  __ Ret();

  __ bind(&return_undefined);
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  __ Drop(argc + 1);
  __ Ret();

  __ bind(&call_builtin);
  __ TailCallExternalReference(
      ExternalReference(Builtins::c_ArrayPop, isolate()), argc + 1, 1);

  // Handle call cache miss.
  __ bind(&miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(function);
}


Handle<Code> CallStubCompiler::CompileStringCharCodeAtCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- r2                     : function name
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // If object is not a string, bail out to regular call.
  if (!object->IsString() || !cell.is_null()) return Handle<Code>::null();

  const int argc = arguments().immediate();
  Label miss;
  Label name_miss;
  Label index_out_of_range;
  Label* index_out_of_range_label = &index_out_of_range;

  if (kind_ == Code::CALL_IC &&
      (CallICBase::StringStubState::decode(extra_state_) ==
       DEFAULT_STRING_STUB)) {
    index_out_of_range_label = &miss;
  }
  GenerateNameCheck(name, &name_miss);

  // Check that the maps starting from the prototype haven't changed.
  GenerateDirectLoadGlobalFunctionPrototype(masm(),
                                            Context::STRING_FUNCTION_INDEX,
                                            r0,
                                            &miss);
  ASSERT(!object.is_identical_to(holder));
  CheckPrototypes(
      Handle<JSObject>(JSObject::cast(object->GetPrototype(isolate()))),
      r0, holder, r1, r3, r4, name, &miss);

  Register receiver = r1;
  Register index = r4;
  Register result = r0;
  __ ldr(receiver, MemOperand(sp, argc * kPointerSize));
  if (argc > 0) {
    __ ldr(index, MemOperand(sp, (argc - 1) * kPointerSize));
  } else {
    __ LoadRoot(index, Heap::kUndefinedValueRootIndex);
  }

  StringCharCodeAtGenerator generator(receiver,
                                      index,
                                      result,
                                      &miss,  // When not a string.
                                      &miss,  // When not a number.
                                      index_out_of_range_label,
                                      STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm());
  __ Drop(argc + 1);
  __ Ret();

  StubRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm(), call_helper);

  if (index_out_of_range.is_linked()) {
    __ bind(&index_out_of_range);
    __ LoadRoot(r0, Heap::kNanValueRootIndex);
    __ Drop(argc + 1);
    __ Ret();
  }

  __ bind(&miss);
  // Restore function name in r2.
  __ Move(r2, name);
  __ bind(&name_miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(function);
}


Handle<Code> CallStubCompiler::CompileStringCharAtCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- r2                     : function name
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  // If object is not a string, bail out to regular call.
  if (!object->IsString() || !cell.is_null()) return Handle<Code>::null();

  const int argc = arguments().immediate();
  Label miss;
  Label name_miss;
  Label index_out_of_range;
  Label* index_out_of_range_label = &index_out_of_range;
  if (kind_ == Code::CALL_IC &&
      (CallICBase::StringStubState::decode(extra_state_) ==
       DEFAULT_STRING_STUB)) {
    index_out_of_range_label = &miss;
  }
  GenerateNameCheck(name, &name_miss);

  // Check that the maps starting from the prototype haven't changed.
  GenerateDirectLoadGlobalFunctionPrototype(masm(),
                                            Context::STRING_FUNCTION_INDEX,
                                            r0,
                                            &miss);
  ASSERT(!object.is_identical_to(holder));
  CheckPrototypes(
      Handle<JSObject>(JSObject::cast(object->GetPrototype(isolate()))),
      r0, holder, r1, r3, r4, name, &miss);

  Register receiver = r0;
  Register index = r4;
  Register scratch = r3;
  Register result = r0;
  __ ldr(receiver, MemOperand(sp, argc * kPointerSize));
  if (argc > 0) {
    __ ldr(index, MemOperand(sp, (argc - 1) * kPointerSize));
  } else {
    __ LoadRoot(index, Heap::kUndefinedValueRootIndex);
  }

  StringCharAtGenerator generator(receiver,
                                  index,
                                  scratch,
                                  result,
                                  &miss,  // When not a string.
                                  &miss,  // When not a number.
                                  index_out_of_range_label,
                                  STRING_INDEX_IS_NUMBER);
  generator.GenerateFast(masm());
  __ Drop(argc + 1);
  __ Ret();

  StubRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm(), call_helper);

  if (index_out_of_range.is_linked()) {
    __ bind(&index_out_of_range);
    __ LoadRoot(r0, Heap::kempty_stringRootIndex);
    __ Drop(argc + 1);
    __ Ret();
  }

  __ bind(&miss);
  // Restore function name in r2.
  __ Move(r2, name);
  __ bind(&name_miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(function);
}


Handle<Code> CallStubCompiler::CompileStringFromCharCodeCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- r2                     : function name
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  const int argc = arguments().immediate();

  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) return Handle<Code>::null();

  Label miss;
  GenerateNameCheck(name, &miss);

  if (cell.is_null()) {
    __ ldr(r1, MemOperand(sp, 1 * kPointerSize));

    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(r1, &miss);

    CheckPrototypes(Handle<JSObject>::cast(object), r1, holder, r0, r3, r4,
                    name, &miss);
  } else {
    ASSERT(cell->value() == *function);
    GenerateGlobalReceiverCheck(Handle<JSObject>::cast(object), holder, name,
                                &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the char code argument.
  Register code = r1;
  __ ldr(code, MemOperand(sp, 0 * kPointerSize));

  // Check the code is a smi.
  Label slow;
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(code, &slow);

  // Convert the smi code to uint16.
  __ and_(code, code, Operand(Smi::FromInt(0xffff)));

  StringCharFromCodeGenerator generator(code, r0);
  generator.GenerateFast(masm());
  __ Drop(argc + 1);
  __ Ret();

  StubRuntimeCallHelper call_helper;
  generator.GenerateSlow(masm(), call_helper);

  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  __ bind(&slow);
  ParameterCount expected(function);
  __ InvokeFunction(function, expected, arguments(),
                    JUMP_FUNCTION, NullCallWrapper(), CALL_AS_METHOD);

  __ bind(&miss);
  // r2: function name.
  GenerateMissBranch();

  // Return the generated code.
  return cell.is_null() ? GetCode(function) : GetCode(Code::NORMAL, name);
}


Handle<Code> CallStubCompiler::CompileMathFloorCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- r2                     : function name
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  const int argc = arguments().immediate();
  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) return Handle<Code>::null();

  Label miss, slow;
  GenerateNameCheck(name, &miss);

  if (cell.is_null()) {
    __ ldr(r1, MemOperand(sp, 1 * kPointerSize));
    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(r1, &miss);
    CheckPrototypes(Handle<JSObject>::cast(object), r1, holder, r0, r3, r4,
                    name, &miss);
  } else {
    ASSERT(cell->value() == *function);
    GenerateGlobalReceiverCheck(Handle<JSObject>::cast(object), holder, name,
                                &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the (only) argument into r0.
  __ ldr(r0, MemOperand(sp, 0 * kPointerSize));

  // If the argument is a smi, just return.
  STATIC_ASSERT(kSmiTag == 0);
  __ tst(r0, Operand(kSmiTagMask));
  __ Drop(argc + 1, eq);
  __ Ret(eq);

  __ CheckMap(r0, r1, Heap::kHeapNumberMapRootIndex, &slow, DONT_DO_SMI_CHECK);

  Label smi_check, just_return;

  // Load the HeapNumber value.
  // We will need access to the value in the core registers, so we load it
  // with ldrd and move it to the fpu. It also spares a sub instruction for
  // updating the HeapNumber value address, as vldr expects a multiple
  // of 4 offset.
  __ Ldrd(r4, r5, FieldMemOperand(r0, HeapNumber::kValueOffset));
  __ vmov(d1, r4, r5);

  // Check for NaN, Infinities and -0.
  // They are invariant through a Math.Floor call, so just
  // return the original argument.
  __ Sbfx(r3, r5, HeapNumber::kExponentShift, HeapNumber::kExponentBits);
  __ cmp(r3, Operand(-1));
  __ b(eq, &just_return);
  __ eor(r3, r5, Operand(0x80000000u));
  __ orr(r3, r3, r4, SetCC);
  __ b(eq, &just_return);
  // Test for values that can be exactly represented as a
  // signed 32-bit integer.
  __ TryDoubleToInt32Exact(r0, d1, d2);
  // If exact, check smi
  __ b(eq, &smi_check);
  __ cmp(r5, Operand(0));

  // If input is in ]+0, +inf[, the cmp has cleared overflow and negative
  // (V=0 and N=0), the two following instructions won't execute and
  // we fall through smi_check to check if the result can fit into a smi.

  // If input is in ]-inf, -0[, sub one and, go to slow if we have
  // an overflow. Else we fall through smi check.
  // Hint: if x is a negative, non integer number,
  // floor(x) <=> round_to_zero(x) - 1.
  __ sub(r0, r0, Operand(1), SetCC, mi);
  __ b(vs, &slow);

  __ bind(&smi_check);
  // Check if the result can fit into an smi. If we had an overflow,
  // the result is either 0x80000000 or 0x7FFFFFFF and won't fit into an smi.
  __ add(r1, r0, Operand(0x40000000), SetCC);
  // If result doesn't fit into an smi, branch to slow.
  __ b(&slow, mi);
  // Tag the result.
  __ mov(r0, Operand(r0, LSL, kSmiTagSize));

  __ bind(&just_return);
  __ Drop(argc + 1);
  __ Ret();

  __ bind(&slow);
  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  ParameterCount expected(function);
  __ InvokeFunction(function, expected, arguments(),
                    JUMP_FUNCTION, NullCallWrapper(), CALL_AS_METHOD);

  __ bind(&miss);
  // r2: function name.
  GenerateMissBranch();

  // Return the generated code.
  return cell.is_null() ? GetCode(function) : GetCode(Code::NORMAL, name);
}


Handle<Code> CallStubCompiler::CompileMathAbsCall(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  // ----------- S t a t e -------------
  //  -- r2                     : function name
  //  -- lr                     : return address
  //  -- sp[(argc - n - 1) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- sp[argc * 4]           : receiver
  // -----------------------------------

  const int argc = arguments().immediate();
  // If the object is not a JSObject or we got an unexpected number of
  // arguments, bail out to the regular call.
  if (!object->IsJSObject() || argc != 1) return Handle<Code>::null();

  Label miss;
  GenerateNameCheck(name, &miss);
  if (cell.is_null()) {
    __ ldr(r1, MemOperand(sp, 1 * kPointerSize));
    STATIC_ASSERT(kSmiTag == 0);
    __ JumpIfSmi(r1, &miss);
    CheckPrototypes(Handle<JSObject>::cast(object), r1, holder, r0, r3, r4,
                    name, &miss);
  } else {
    ASSERT(cell->value() == *function);
    GenerateGlobalReceiverCheck(Handle<JSObject>::cast(object), holder, name,
                                &miss);
    GenerateLoadFunctionFromCell(cell, function, &miss);
  }

  // Load the (only) argument into r0.
  __ ldr(r0, MemOperand(sp, 0 * kPointerSize));

  // Check if the argument is a smi.
  Label not_smi;
  STATIC_ASSERT(kSmiTag == 0);
  __ JumpIfNotSmi(r0, &not_smi);

  // Do bitwise not or do nothing depending on the sign of the
  // argument.
  __ eor(r1, r0, Operand(r0, ASR, kBitsPerInt - 1));

  // Add 1 or do nothing depending on the sign of the argument.
  __ sub(r0, r1, Operand(r0, ASR, kBitsPerInt - 1), SetCC);

  // If the result is still negative, go to the slow case.
  // This only happens for the most negative smi.
  Label slow;
  __ b(mi, &slow);

  // Smi case done.
  __ Drop(argc + 1);
  __ Ret();

  // Check if the argument is a heap number and load its exponent and
  // sign.
  __ bind(&not_smi);
  __ CheckMap(r0, r1, Heap::kHeapNumberMapRootIndex, &slow, DONT_DO_SMI_CHECK);
  __ ldr(r1, FieldMemOperand(r0, HeapNumber::kExponentOffset));

  // Check the sign of the argument. If the argument is positive,
  // just return it.
  Label negative_sign;
  __ tst(r1, Operand(HeapNumber::kSignMask));
  __ b(ne, &negative_sign);
  __ Drop(argc + 1);
  __ Ret();

  // If the argument is negative, clear the sign, and return a new
  // number.
  __ bind(&negative_sign);
  __ eor(r1, r1, Operand(HeapNumber::kSignMask));
  __ ldr(r3, FieldMemOperand(r0, HeapNumber::kMantissaOffset));
  __ LoadRoot(r6, Heap::kHeapNumberMapRootIndex);
  __ AllocateHeapNumber(r0, r4, r5, r6, &slow);
  __ str(r1, FieldMemOperand(r0, HeapNumber::kExponentOffset));
  __ str(r3, FieldMemOperand(r0, HeapNumber::kMantissaOffset));
  __ Drop(argc + 1);
  __ Ret();

  // Tail call the full function. We do not have to patch the receiver
  // because the function makes no use of it.
  __ bind(&slow);
  ParameterCount expected(function);
  __ InvokeFunction(function, expected, arguments(),
                    JUMP_FUNCTION, NullCallWrapper(), CALL_AS_METHOD);

  __ bind(&miss);
  // r2: function name.
  GenerateMissBranch();

  // Return the generated code.
  return cell.is_null() ? GetCode(function) : GetCode(Code::NORMAL, name);
}


Handle<Code> CallStubCompiler::CompileFastApiCall(
    const CallOptimization& optimization,
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<String> name) {
  Counters* counters = isolate()->counters();

  ASSERT(optimization.is_simple_api_call());
  // Bail out if object is a global object as we don't want to
  // repatch it to global receiver.
  if (object->IsGlobalObject()) return Handle<Code>::null();
  if (!cell.is_null()) return Handle<Code>::null();
  if (!object->IsJSObject()) return Handle<Code>::null();
  int depth = optimization.GetPrototypeDepthOfExpectedType(
      Handle<JSObject>::cast(object), holder);
  if (depth == kInvalidProtoDepth) return Handle<Code>::null();

  Label miss, miss_before_stack_reserved;
  GenerateNameCheck(name, &miss_before_stack_reserved);

  // Get the receiver from the stack.
  const int argc = arguments().immediate();
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(r1, &miss_before_stack_reserved);

  __ IncrementCounter(counters->call_const(), 1, r0, r3);
  __ IncrementCounter(counters->call_const_fast_api(), 1, r0, r3);

  ReserveSpaceForFastApiCall(masm(), r0);

  // Check that the maps haven't changed and find a Holder as a side effect.
  CheckPrototypes(Handle<JSObject>::cast(object), r1, holder, r0, r3, r4, name,
                  depth, &miss);

  GenerateFastApiDirectCall(masm(), optimization, argc);

  __ bind(&miss);
  FreeSpaceForFastApiCall(masm());

  __ bind(&miss_before_stack_reserved);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(function);
}


void CallStubCompiler::CompileHandlerFrontend(Handle<Object> object,
                                              Handle<JSObject> holder,
                                              Handle<Name> name,
                                              CheckType check,
                                              Label* success) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;
  GenerateNameCheck(name, &miss);

  // Get the receiver from the stack
  const int argc = arguments().immediate();
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));

  // Check that the receiver isn't a smi.
  if (check != NUMBER_CHECK) {
    __ JumpIfSmi(r1, &miss);
  }

  // Make sure that it's okay not to patch the on stack receiver
  // unless we're doing a receiver map check.
  ASSERT(!object->IsGlobalObject() || check == RECEIVER_MAP_CHECK);
  switch (check) {
    case RECEIVER_MAP_CHECK:
      __ IncrementCounter(isolate()->counters()->call_const(), 1, r0, r3);

      // Check that the maps haven't changed.
      CheckPrototypes(Handle<JSObject>::cast(object), r1, holder, r0, r3, r4,
                      name, &miss);

      // Patch the receiver on the stack with the global proxy if
      // necessary.
      if (object->IsGlobalObject()) {
        __ ldr(r3, FieldMemOperand(r1, GlobalObject::kGlobalReceiverOffset));
        __ str(r3, MemOperand(sp, argc * kPointerSize));
      }
      break;

    case STRING_CHECK:
      // Check that the object is a string.
      __ CompareObjectType(r1, r3, r3, FIRST_NONSTRING_TYPE);
      __ b(ge, &miss);
      // Check that the maps starting from the prototype haven't changed.
      GenerateDirectLoadGlobalFunctionPrototype(
          masm(), Context::STRING_FUNCTION_INDEX, r0, &miss);
      CheckPrototypes(
          Handle<JSObject>(JSObject::cast(object->GetPrototype(isolate()))),
          r0, holder, r3, r1, r4, name, &miss);
      break;

    case SYMBOL_CHECK:
      // Check that the object is a symbol.
      __ CompareObjectType(r1, r1, r3, SYMBOL_TYPE);
      __ b(ne, &miss);
      // Check that the maps starting from the prototype haven't changed.
      GenerateDirectLoadGlobalFunctionPrototype(
          masm(), Context::SYMBOL_FUNCTION_INDEX, r0, &miss);
      CheckPrototypes(
          Handle<JSObject>(JSObject::cast(object->GetPrototype(isolate()))),
          r0, holder, r3, r1, r4, name, &miss);
      break;

    case NUMBER_CHECK: {
      Label fast;
      // Check that the object is a smi or a heap number.
      __ JumpIfSmi(r1, &fast);
      __ CompareObjectType(r1, r0, r0, HEAP_NUMBER_TYPE);
      __ b(ne, &miss);
      __ bind(&fast);
      // Check that the maps starting from the prototype haven't changed.
      GenerateDirectLoadGlobalFunctionPrototype(
          masm(), Context::NUMBER_FUNCTION_INDEX, r0, &miss);
      CheckPrototypes(
          Handle<JSObject>(JSObject::cast(object->GetPrototype(isolate()))),
          r0, holder, r3, r1, r4, name, &miss);
      break;
    }
    case BOOLEAN_CHECK: {
      Label fast;
      // Check that the object is a boolean.
      __ LoadRoot(ip, Heap::kTrueValueRootIndex);
      __ cmp(r1, ip);
      __ b(eq, &fast);
      __ LoadRoot(ip, Heap::kFalseValueRootIndex);
      __ cmp(r1, ip);
      __ b(ne, &miss);
      __ bind(&fast);
      // Check that the maps starting from the prototype haven't changed.
      GenerateDirectLoadGlobalFunctionPrototype(
          masm(), Context::BOOLEAN_FUNCTION_INDEX, r0, &miss);
      CheckPrototypes(
          Handle<JSObject>(JSObject::cast(object->GetPrototype(isolate()))),
          r0, holder, r3, r1, r4, name, &miss);
      break;
    }
  }

  __ b(success);

  // Handle call cache miss.
  __ bind(&miss);
  GenerateMissBranch();
}


void CallStubCompiler::CompileHandlerBackend(Handle<JSFunction> function) {
  CallKind call_kind = CallICBase::Contextual::decode(extra_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  ParameterCount expected(function);
  __ InvokeFunction(function, expected, arguments(),
                    JUMP_FUNCTION, NullCallWrapper(), call_kind);
}


Handle<Code> CallStubCompiler::CompileCallConstant(
    Handle<Object> object,
    Handle<JSObject> holder,
    Handle<Name> name,
    CheckType check,
    Handle<JSFunction> function) {
  if (HasCustomCallGenerator(function)) {
    Handle<Code> code = CompileCustomCall(object, holder,
                                          Handle<JSGlobalPropertyCell>::null(),
                                          function, Handle<String>::cast(name));
    // A null handle means bail out to the regular compiler code below.
    if (!code.is_null()) return code;
  }

  Label success;

  CompileHandlerFrontend(object, holder, name, check, &success);
  __ bind(&success);
  CompileHandlerBackend(function);

  // Return the generated code.
  return GetCode(function);
}


Handle<Code> CallStubCompiler::CompileCallInterceptor(Handle<JSObject> object,
                                                      Handle<JSObject> holder,
                                                      Handle<Name> name) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;
  GenerateNameCheck(name, &miss);

  // Get the number of arguments.
  const int argc = arguments().immediate();
  LookupResult lookup(isolate());
  LookupPostInterceptor(holder, name, &lookup);

  // Get the receiver from the stack.
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));

  CallInterceptorCompiler compiler(this, arguments(), r2, extra_state_);
  compiler.Compile(masm(), object, holder, name, &lookup, r1, r3, r4, r0,
                   &miss);

  // Move returned value, the function to call, to r1.
  __ mov(r1, r0);
  // Restore receiver.
  __ ldr(r0, MemOperand(sp, argc * kPointerSize));

  GenerateCallFunction(masm(), object, arguments(), &miss, extra_state_);

  // Handle call cache miss.
  __ bind(&miss);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(Code::INTERCEPTOR, name);
}


Handle<Code> CallStubCompiler::CompileCallGlobal(
    Handle<JSObject> object,
    Handle<GlobalObject> holder,
    Handle<JSGlobalPropertyCell> cell,
    Handle<JSFunction> function,
    Handle<Name> name) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  if (HasCustomCallGenerator(function)) {
    Handle<Code> code = CompileCustomCall(
        object, holder, cell, function, Handle<String>::cast(name));
    // A null handle means bail out to the regular compiler code below.
    if (!code.is_null()) return code;
  }

  Label miss;
  GenerateNameCheck(name, &miss);

  // Get the number of arguments.
  const int argc = arguments().immediate();
  GenerateGlobalReceiverCheck(object, holder, name, &miss);
  GenerateLoadFunctionFromCell(cell, function, &miss);

  // Patch the receiver on the stack with the global proxy if
  // necessary.
  if (object->IsGlobalObject()) {
    __ ldr(r3, FieldMemOperand(r0, GlobalObject::kGlobalReceiverOffset));
    __ str(r3, MemOperand(sp, argc * kPointerSize));
  }

  // Set up the context (function already in r1).
  __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));

  // Jump to the cached code (tail call).
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->call_global_inline(), 1, r3, r4);
  ParameterCount expected(function->shared()->formal_parameter_count());
  CallKind call_kind = CallICBase::Contextual::decode(extra_state_)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  // We call indirectly through the code field in the function to
  // allow recompilation to take effect without changing any of the
  // call sites.
  __ ldr(r3, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
  __ InvokeCode(r3, expected, arguments(), JUMP_FUNCTION,
                NullCallWrapper(), call_kind);

  // Handle call cache miss.
  __ bind(&miss);
  __ IncrementCounter(counters->call_global_inline_miss(), 1, r1, r3);
  GenerateMissBranch();

  // Return the generated code.
  return GetCode(Code::NORMAL, name);
}


Handle<Code> StoreStubCompiler::CompileStoreCallback(
    Handle<Name> name,
    Handle<JSObject> object,
    Handle<JSObject> holder,
    Handle<ExecutableAccessorInfo> callback) {
  Label miss;
  // Check that the maps haven't changed.
  __ JumpIfSmi(receiver(), &miss);
  CheckPrototypes(object, receiver(), holder,
                  scratch1(), scratch2(), scratch3(), name, &miss);

  // Stub never generated for non-global objects that require access checks.
  ASSERT(holder->IsJSGlobalProxy() || !holder->IsAccessCheckNeeded());

  __ push(receiver());  // receiver
  __ mov(ip, Operand(callback));  // callback info
  __ Push(ip, this->name(), value());

  // Do tail-call to the runtime system.
  ExternalReference store_callback_property =
      ExternalReference(IC_Utility(IC::kStoreCallbackProperty), isolate());
  __ TailCallExternalReference(store_callback_property, 4, 1);

  // Handle store cache miss.
  __ bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  return GetICCode(kind(), Code::CALLBACKS, name);
}


#undef __
#define __ ACCESS_MASM(masm)


void StoreStubCompiler::GenerateStoreViaSetter(
    MacroAssembler* masm,
    Handle<JSFunction> setter) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Save value register, so we can restore it later.
    __ push(r0);

    if (!setter.is_null()) {
      // Call the JavaScript setter with receiver and value on the stack.
      __ Push(r1, r0);
      ParameterCount actual(1);
      ParameterCount expected(setter);
      __ InvokeFunction(setter, expected, actual,
                        CALL_FUNCTION, NullCallWrapper(), CALL_AS_METHOD);
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetSetterStubDeoptPCOffset(masm->pc_offset());
    }

    // We have to return the passed value, not the return value of the setter.
    __ pop(r0);

    // Restore context register.
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  }
  __ Ret();
}


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> StoreStubCompiler::CompileStoreInterceptor(
    Handle<JSObject> object,
    Handle<Name> name) {
  Label miss;

  // Check that the map of the object hasn't changed.
  __ CheckMap(receiver(), scratch1(), Handle<Map>(object->map()), &miss,
              DO_SMI_CHECK, ALLOW_ELEMENT_TRANSITION_MAPS);

  // Perform global security token check if needed.
  if (object->IsJSGlobalProxy()) {
    __ CheckAccessGlobalProxy(receiver(), scratch1(), &miss);
  }

  // Stub is never generated for non-global objects that require access
  // checks.
  ASSERT(object->IsJSGlobalProxy() || !object->IsAccessCheckNeeded());

  __ Push(receiver(), this->name(), value());

  __ mov(scratch1(), Operand(Smi::FromInt(strict_mode())));
  __ push(scratch1());  // strict mode

  // Do tail-call to the runtime system.
  ExternalReference store_ic_property =
      ExternalReference(IC_Utility(IC::kStoreInterceptorProperty), isolate());
  __ TailCallExternalReference(store_ic_property, 4, 1);

  // Handle store cache miss.
  __ bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  return GetICCode(kind(), Code::INTERCEPTOR, name);
}


Handle<Code> StoreStubCompiler::CompileStoreGlobal(
    Handle<GlobalObject> object,
    Handle<JSGlobalPropertyCell> cell,
    Handle<Name> name) {
  Label miss;

  // Check that the map of the global has not changed.
  __ ldr(scratch1(), FieldMemOperand(receiver(), HeapObject::kMapOffset));
  __ cmp(scratch1(), Operand(Handle<Map>(object->map())));
  __ b(ne, &miss);

  // Check that the value in the cell is not the hole. If it is, this
  // cell could have been deleted and reintroducing the global needs
  // to update the property details in the property dictionary of the
  // global object. We bail out to the runtime system to do that.
  __ mov(scratch1(), Operand(cell));
  __ LoadRoot(scratch2(), Heap::kTheHoleValueRootIndex);
  __ ldr(scratch3(),
         FieldMemOperand(scratch1(), JSGlobalPropertyCell::kValueOffset));
  __ cmp(scratch3(), scratch2());
  __ b(eq, &miss);

  // Store the value in the cell.
  __ str(value(),
         FieldMemOperand(scratch1(), JSGlobalPropertyCell::kValueOffset));
  // Cells are always rescanned, so no write barrier here.

  Counters* counters = isolate()->counters();
  __ IncrementCounter(
      counters->named_store_global_inline(), 1, scratch1(), scratch2());
  __ Ret();

  // Handle store cache miss.
  __ bind(&miss);
  __ IncrementCounter(
      counters->named_store_global_inline_miss(), 1, scratch1(), scratch2());
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  return GetICCode(kind(), Code::NORMAL, name);
}


Handle<Code> LoadStubCompiler::CompileLoadNonexistent(
    Handle<JSObject> object,
    Handle<JSObject> last,
    Handle<Name> name,
    Handle<GlobalObject> global) {
  Label success;

  NonexistentHandlerFrontend(object, last, name, &success, global);

  __ bind(&success);
  // Return undefined if maps of the full prototype chain are still the
  // same and no global property with this name contains a value.
  __ LoadRoot(r0, Heap::kUndefinedValueRootIndex);
  __ Ret();

  // Return the generated code.
  return GetCode(kind(), Code::NONEXISTENT, name);
}


Register* LoadStubCompiler::registers() {
  // receiver, name, scratch1, scratch2, scratch3, scratch4.
  static Register registers[] = { r0, r2, r3, r1, r4, r5 };
  return registers;
}


Register* KeyedLoadStubCompiler::registers() {
  // receiver, name, scratch1, scratch2, scratch3, scratch4.
  static Register registers[] = { r1, r0, r2, r3, r4, r5 };
  return registers;
}


Register* StoreStubCompiler::registers() {
  // receiver, name, value, scratch1, scratch2, scratch3.
  static Register registers[] = { r1, r2, r0, r3, r4, r5 };
  return registers;
}


Register* KeyedStoreStubCompiler::registers() {
  // receiver, name, value, scratch1, scratch2, scratch3.
  static Register registers[] = { r2, r1, r0, r3, r4, r5 };
  return registers;
}


void KeyedLoadStubCompiler::GenerateNameCheck(Handle<Name> name,
                                              Register name_reg,
                                              Label* miss) {
  __ cmp(name_reg, Operand(name));
  __ b(ne, miss);
}


void KeyedStoreStubCompiler::GenerateNameCheck(Handle<Name> name,
                                               Register name_reg,
                                               Label* miss) {
  __ cmp(name_reg, Operand(name));
  __ b(ne, miss);
}


#undef __
#define __ ACCESS_MASM(masm)


void LoadStubCompiler::GenerateLoadViaGetter(MacroAssembler* masm,
                                             Handle<JSFunction> getter) {
  // ----------- S t a t e -------------
  //  -- r0    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    if (!getter.is_null()) {
      // Call the JavaScript getter with the receiver on the stack.
      __ push(r0);
      ParameterCount actual(0);
      ParameterCount expected(getter);
      __ InvokeFunction(getter, expected, actual,
                        CALL_FUNCTION, NullCallWrapper(), CALL_AS_METHOD);
    } else {
      // If we generate a global code snippet for deoptimization only, remember
      // the place to continue after deoptimization.
      masm->isolate()->heap()->SetGetterStubDeoptPCOffset(masm->pc_offset());
    }

    // Restore context register.
    __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
  }
  __ Ret();
}


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> LoadStubCompiler::CompileLoadGlobal(
    Handle<JSObject> object,
    Handle<GlobalObject> global,
    Handle<JSGlobalPropertyCell> cell,
    Handle<Name> name,
    bool is_dont_delete) {
  Label success, miss;

  __ CheckMap(
      receiver(), scratch1(), Handle<Map>(object->map()), &miss, DO_SMI_CHECK);
  HandlerFrontendHeader(
      object, receiver(), Handle<JSObject>::cast(global), name, &miss);

  // Get the value from the cell.
  __ mov(r3, Operand(cell));
  __ ldr(r4, FieldMemOperand(r3, JSGlobalPropertyCell::kValueOffset));

  // Check for deleted property if property can actually be deleted.
  if (!is_dont_delete) {
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmp(r4, ip);
    __ b(eq, &miss);
  }

  HandlerFrontendFooter(&success, &miss);
  __ bind(&success);

  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->named_load_global_stub(), 1, r1, r3);
  __ mov(r0, r4);
  __ Ret();

  // Return the generated code.
  return GetICCode(kind(), Code::NORMAL, name);
}


Handle<Code> BaseLoadStubCompiler::CompilePolymorphicIC(
    MapHandleList* receiver_maps,
    CodeHandleList* handlers,
    Handle<Name> name,
    Code::StubType type,
    IcCheckType check) {
  Label miss;

  if (check == PROPERTY) {
    GenerateNameCheck(name, this->name(), &miss);
  }

  __ JumpIfSmi(receiver(), &miss);
  Register map_reg = scratch1();

  int receiver_count = receiver_maps->length();
  int number_of_handled_maps = 0;
  __ ldr(map_reg, FieldMemOperand(receiver(), HeapObject::kMapOffset));
  for (int current = 0; current < receiver_count; ++current) {
    Handle<Map> map = receiver_maps->at(current);
    if (!map->is_deprecated()) {
      number_of_handled_maps++;
      __ mov(ip, Operand(receiver_maps->at(current)));
      __ cmp(map_reg, ip);
      __ Jump(handlers->at(current), RelocInfo::CODE_TARGET, eq);
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


Handle<Code> KeyedStoreStubCompiler::CompileStorePolymorphic(
    MapHandleList* receiver_maps,
    CodeHandleList* handler_stubs,
    MapHandleList* transitioned_maps) {
  Label miss;
  __ JumpIfSmi(receiver(), &miss);

  int receiver_count = receiver_maps->length();
  __ ldr(scratch1(), FieldMemOperand(receiver(), HeapObject::kMapOffset));
  for (int i = 0; i < receiver_count; ++i) {
    __ mov(ip, Operand(receiver_maps->at(i)));
    __ cmp(scratch1(), ip);
    if (transitioned_maps->at(i).is_null()) {
      __ Jump(handler_stubs->at(i), RelocInfo::CODE_TARGET, eq);
    } else {
      Label next_map;
      __ b(ne, &next_map);
      __ mov(transition_map(), Operand(transitioned_maps->at(i)));
      __ Jump(handler_stubs->at(i), RelocInfo::CODE_TARGET, al);
      __ bind(&next_map);
    }
  }

  __ bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  return GetICCode(
      kind(), Code::NORMAL, factory()->empty_string(), POLYMORPHIC);
}


Handle<Code> ConstructStubCompiler::CompileConstructStub(
    Handle<JSFunction> function) {
  // ----------- S t a t e -------------
  //  -- r0    : argc
  //  -- r1    : constructor
  //  -- lr    : return address
  //  -- [sp]  : last argument
  // -----------------------------------
  Label generic_stub_call;

  // Use r7 for holding undefined which is used in several places below.
  __ LoadRoot(r7, Heap::kUndefinedValueRootIndex);

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Check to see whether there are any break points in the function code. If
  // there are jump to the generic constructor stub which calls the actual
  // code for the function thereby hitting the break points.
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(r2, FieldMemOperand(r2, SharedFunctionInfo::kDebugInfoOffset));
  __ cmp(r2, r7);
  __ b(ne, &generic_stub_call);
#endif

  // Load the initial map and verify that it is in fact a map.
  // r1: constructor function
  // r7: undefined
  __ ldr(r2, FieldMemOperand(r1, JSFunction::kPrototypeOrInitialMapOffset));
  __ JumpIfSmi(r2, &generic_stub_call);
  __ CompareObjectType(r2, r3, r4, MAP_TYPE);
  __ b(ne, &generic_stub_call);

#ifdef DEBUG
  // Cannot construct functions this way.
  // r0: argc
  // r1: constructor function
  // r2: initial map
  // r7: undefined
  __ CompareInstanceType(r2, r3, JS_FUNCTION_TYPE);
  __ Check(ne, "Function constructed by construct stub.");
#endif

  // Now allocate the JSObject in new space.
  // r0: argc
  // r1: constructor function
  // r2: initial map
  // r7: undefined
  ASSERT(function->has_initial_map());
  __ ldrb(r3, FieldMemOperand(r2, Map::kInstanceSizeOffset));
#ifdef DEBUG
  int instance_size = function->initial_map()->instance_size();
  __ cmp(r3, Operand(instance_size >> kPointerSizeLog2));
  __ Check(eq, "Instance size of initial map changed.");
#endif
  __ Allocate(r3, r4, r5, r6, &generic_stub_call, SIZE_IN_WORDS);

  // Allocated the JSObject, now initialize the fields. Map is set to initial
  // map and properties and elements are set to empty fixed array.
  // r0: argc
  // r1: constructor function
  // r2: initial map
  // r3: object size (in words)
  // r4: JSObject (not tagged)
  // r7: undefined
  __ LoadRoot(r6, Heap::kEmptyFixedArrayRootIndex);
  __ mov(r5, r4);
  ASSERT_EQ(0 * kPointerSize, JSObject::kMapOffset);
  __ str(r2, MemOperand(r5, kPointerSize, PostIndex));
  ASSERT_EQ(1 * kPointerSize, JSObject::kPropertiesOffset);
  __ str(r6, MemOperand(r5, kPointerSize, PostIndex));
  ASSERT_EQ(2 * kPointerSize, JSObject::kElementsOffset);
  __ str(r6, MemOperand(r5, kPointerSize, PostIndex));

  // Calculate the location of the first argument. The stack contains only the
  // argc arguments.
  __ add(r1, sp, Operand(r0, LSL, kPointerSizeLog2));

  // Fill all the in-object properties with undefined.
  // r0: argc
  // r1: first argument
  // r3: object size (in words)
  // r4: JSObject (not tagged)
  // r5: First in-object property of JSObject (not tagged)
  // r7: undefined
  // Fill the initialized properties with a constant value or a passed argument
  // depending on the this.x = ...; assignment in the function.
  Handle<SharedFunctionInfo> shared(function->shared());
  for (int i = 0; i < shared->this_property_assignments_count(); i++) {
    if (shared->IsThisPropertyAssignmentArgument(i)) {
      Label not_passed, next;
      // Check if the argument assigned to the property is actually passed.
      int arg_number = shared->GetThisPropertyAssignmentArgument(i);
      __ cmp(r0, Operand(arg_number));
      __ b(le, &not_passed);
      // Argument passed - find it on the stack.
      __ ldr(r2, MemOperand(r1, (arg_number + 1) * -kPointerSize));
      __ str(r2, MemOperand(r5, kPointerSize, PostIndex));
      __ b(&next);
      __ bind(&not_passed);
      // Set the property to undefined.
      __ str(r7, MemOperand(r5, kPointerSize, PostIndex));
      __ bind(&next);
    } else {
      // Set the property to the constant value.
      Handle<Object> constant(shared->GetThisPropertyAssignmentConstant(i),
                              isolate());
      __ mov(r2, Operand(constant));
      __ str(r2, MemOperand(r5, kPointerSize, PostIndex));
    }
  }

  // Fill the unused in-object property fields with undefined.
  for (int i = shared->this_property_assignments_count();
       i < function->initial_map()->inobject_properties();
       i++) {
      __ str(r7, MemOperand(r5, kPointerSize, PostIndex));
  }

  // r0: argc
  // r4: JSObject (not tagged)
  // Move argc to r1 and the JSObject to return to r0 and tag it.
  __ mov(r1, r0);
  __ mov(r0, r4);
  __ orr(r0, r0, Operand(kHeapObjectTag));

  // r0: JSObject
  // r1: argc
  // Remove caller arguments and receiver from the stack and return.
  __ add(sp, sp, Operand(r1, LSL, kPointerSizeLog2));
  __ add(sp, sp, Operand(kPointerSize));
  Counters* counters = isolate()->counters();
  __ IncrementCounter(counters->constructed_objects(), 1, r1, r2);
  __ IncrementCounter(counters->constructed_objects_stub(), 1, r1, r2);
  __ Jump(lr);

  // Jump to the generic stub in case the specialized code cannot handle the
  // construction.
  __ bind(&generic_stub_call);
  Handle<Code> code = isolate()->builtins()->JSConstructStubGeneric();
  __ Jump(code, RelocInfo::CODE_TARGET);

  // Return the generated code.
  return GetCode();
}


#undef __
#define __ ACCESS_MASM(masm)


void KeyedLoadStubCompiler::GenerateLoadDictionaryElement(
    MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------
  Label slow, miss_force_generic;

  Register key = r0;
  Register receiver = r1;

  __ JumpIfNotSmi(key, &miss_force_generic);
  __ mov(r2, Operand(key, ASR, kSmiTagSize));
  __ ldr(r4, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ LoadFromNumberDictionary(&slow, r4, key, r0, r2, r3, r5);
  __ Ret();

  __ bind(&slow);
  __ IncrementCounter(
      masm->isolate()->counters()->keyed_load_external_array_slow(),
      1, r2, r3);

  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------
  TailCallBuiltin(masm, Builtins::kKeyedLoadIC_Slow);

  // Miss case, call the runtime.
  __ bind(&miss_force_generic);

  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------
  TailCallBuiltin(masm, Builtins::kKeyedLoadIC_MissForceGeneric);
}


static void GenerateSmiKeyCheck(MacroAssembler* masm,
                                Register key,
                                Register scratch0,
                                Register scratch1,
                                DwVfpRegister double_scratch0,
                                DwVfpRegister double_scratch1,
                                Label* fail) {
  Label key_ok;
  // Check for smi or a smi inside a heap number.  We convert the heap
  // number and check if the conversion is exact and fits into the smi
  // range.
  __ JumpIfSmi(key, &key_ok);
  __ CheckMap(key,
              scratch0,
              Heap::kHeapNumberMapRootIndex,
              fail,
              DONT_DO_SMI_CHECK);
  __ sub(ip, key, Operand(kHeapObjectTag));
  __ vldr(double_scratch0, ip, HeapNumber::kValueOffset);
  __ TryDoubleToInt32Exact(scratch0, double_scratch0, double_scratch1);
  __ b(ne, fail);
  __ TrySmiTag(scratch0, fail, scratch1);
  __ mov(key, scratch0);
  __ bind(&key_ok);
}


void KeyedStoreStubCompiler::GenerateStoreExternalArray(
    MacroAssembler* masm,
    ElementsKind elements_kind) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- r1     : key
  //  -- r2     : receiver
  //  -- lr     : return address
  // -----------------------------------
  Label slow, check_heap_number, miss_force_generic;

  // Register usage.
  Register value = r0;
  Register key = r1;
  Register receiver = r2;
  // r3 mostly holds the elements array or the destination external array.

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, key, r4, r5, d1, d2, &miss_force_generic);

  __ ldr(r3, FieldMemOperand(receiver, JSObject::kElementsOffset));

  // Check that the index is in range
  __ ldr(ip, FieldMemOperand(r3, ExternalArray::kLengthOffset));
  __ cmp(key, ip);
  // Unsigned comparison catches both negative and too-large values.
  __ b(hs, &miss_force_generic);

  // Handle both smis and HeapNumbers in the fast path. Go to the
  // runtime for all other kinds of values.
  // r3: external array.
  if (elements_kind == EXTERNAL_PIXEL_ELEMENTS) {
    // Double to pixel conversion is only implemented in the runtime for now.
    __ JumpIfNotSmi(value, &slow);
  } else {
    __ JumpIfNotSmi(value, &check_heap_number);
  }
  __ SmiUntag(r5, value);
  __ ldr(r3, FieldMemOperand(r3, ExternalArray::kExternalPointerOffset));

  // r3: base pointer of external storage.
  // r5: value (integer).
  switch (elements_kind) {
    case EXTERNAL_PIXEL_ELEMENTS:
      // Clamp the value to [0..255].
      __ Usat(r5, 8, Operand(r5));
      __ strb(r5, MemOperand(r3, key, LSR, 1));
      break;
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
      __ strb(r5, MemOperand(r3, key, LSR, 1));
      break;
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
      __ strh(r5, MemOperand(r3, key, LSL, 0));
      break;
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
      __ str(r5, MemOperand(r3, key, LSL, 1));
      break;
    case EXTERNAL_FLOAT_ELEMENTS:
      // Perform int-to-float conversion and store to memory.
      __ SmiUntag(r4, key);
      StoreIntAsFloat(masm, r3, r4, r5, r7);
      break;
    case EXTERNAL_DOUBLE_ELEMENTS:
      __ vmov(s2, r5);
      __ vcvt_f64_s32(d0, s2);
      __ add(r3, r3, Operand(key, LSL, 2));
      // r3: effective address of the double element
      __ vstr(d0, r3, 0);
      break;
    case FAST_ELEMENTS:
    case FAST_SMI_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case DICTIONARY_ELEMENTS:
    case NON_STRICT_ARGUMENTS_ELEMENTS:
      UNREACHABLE();
      break;
  }

  // Entry registers are intact, r0 holds the value which is the return value.
  __ Ret();

  if (elements_kind != EXTERNAL_PIXEL_ELEMENTS) {
    // r3: external array.
    __ bind(&check_heap_number);
    __ CompareObjectType(value, r5, r6, HEAP_NUMBER_TYPE);
    __ b(ne, &slow);

    __ ldr(r3, FieldMemOperand(r3, ExternalArray::kExternalPointerOffset));

    // r3: base pointer of external storage.

    // The WebGL specification leaves the behavior of storing NaN and
    // +/-Infinity into integer arrays basically undefined. For more
    // reproducible behavior, convert these to zero.

    if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
      // vldr requires offset to be a multiple of 4 so we can not
      // include -kHeapObjectTag into it.
      __ sub(r5, r0, Operand(kHeapObjectTag));
      __ vldr(d0, r5, HeapNumber::kValueOffset);
      __ add(r5, r3, Operand(key, LSL, 1));
      __ vcvt_f32_f64(s0, d0);
      __ vstr(s0, r5, 0);
    } else if (elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
      __ sub(r5, r0, Operand(kHeapObjectTag));
      __ vldr(d0, r5, HeapNumber::kValueOffset);
      __ add(r5, r3, Operand(key, LSL, 2));
      __ vstr(d0, r5, 0);
    } else {
      // Hoisted load.  vldr requires offset to be a multiple of 4 so we can
      // not include -kHeapObjectTag into it.
      __ sub(r5, value, Operand(kHeapObjectTag));
      __ vldr(d0, r5, HeapNumber::kValueOffset);
      __ ECMAToInt32(r5, d0, r6, r7, r9, d1);

      switch (elements_kind) {
        case EXTERNAL_BYTE_ELEMENTS:
        case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
          __ strb(r5, MemOperand(r3, key, LSR, 1));
          break;
        case EXTERNAL_SHORT_ELEMENTS:
        case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
          __ strh(r5, MemOperand(r3, key, LSL, 0));
          break;
        case EXTERNAL_INT_ELEMENTS:
        case EXTERNAL_UNSIGNED_INT_ELEMENTS:
          __ str(r5, MemOperand(r3, key, LSL, 1));
          break;
        case EXTERNAL_PIXEL_ELEMENTS:
        case EXTERNAL_FLOAT_ELEMENTS:
        case EXTERNAL_DOUBLE_ELEMENTS:
        case FAST_ELEMENTS:
        case FAST_SMI_ELEMENTS:
        case FAST_DOUBLE_ELEMENTS:
        case FAST_HOLEY_ELEMENTS:
        case FAST_HOLEY_SMI_ELEMENTS:
        case FAST_HOLEY_DOUBLE_ELEMENTS:
        case DICTIONARY_ELEMENTS:
        case NON_STRICT_ARGUMENTS_ELEMENTS:
          UNREACHABLE();
          break;
      }
    }

    // Entry registers are intact, r0 holds the value which is the return
    // value.
    __ Ret();
  }

  // Slow case, key and receiver still in r0 and r1.
  __ bind(&slow);
  __ IncrementCounter(
      masm->isolate()->counters()->keyed_load_external_array_slow(),
      1, r2, r3);

  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------
  TailCallBuiltin(masm, Builtins::kKeyedStoreIC_Slow);

  // Miss case, call the runtime.
  __ bind(&miss_force_generic);

  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------
  TailCallBuiltin(masm, Builtins::kKeyedStoreIC_MissForceGeneric);
}


void KeyedStoreStubCompiler::GenerateStoreFastElement(
    MacroAssembler* masm,
    bool is_js_array,
    ElementsKind elements_kind,
    KeyedAccessStoreMode store_mode) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : key
  //  -- r2    : receiver
  //  -- lr    : return address
  //  -- r3    : scratch
  //  -- r4    : scratch (elements)
  // -----------------------------------
  Label miss_force_generic, transition_elements_kind, grow, slow;
  Label finish_store, check_capacity;

  Register value_reg = r0;
  Register key_reg = r1;
  Register receiver_reg = r2;
  Register scratch = r4;
  Register elements_reg = r3;
  Register length_reg = r5;
  Register scratch2 = r6;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, key_reg, r4, r5, d1, d2, &miss_force_generic);

  if (IsFastSmiElementsKind(elements_kind)) {
    __ JumpIfNotSmi(value_reg, &transition_elements_kind);
  }

  // Check that the key is within bounds.
  __ ldr(elements_reg,
         FieldMemOperand(receiver_reg, JSObject::kElementsOffset));
  if (is_js_array) {
    __ ldr(scratch, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
  } else {
    __ ldr(scratch, FieldMemOperand(elements_reg, FixedArray::kLengthOffset));
  }
  // Compare smis.
  __ cmp(key_reg, scratch);
  if (is_js_array && IsGrowStoreMode(store_mode)) {
    __ b(hs, &grow);
  } else {
    __ b(hs, &miss_force_generic);
  }

  // Make sure elements is a fast element array, not 'cow'.
  __ CheckMap(elements_reg,
              scratch,
              Heap::kFixedArrayMapRootIndex,
              &miss_force_generic,
              DONT_DO_SMI_CHECK);

  __ bind(&finish_store);
  if (IsFastSmiElementsKind(elements_kind)) {
    __ add(scratch,
           elements_reg,
           Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
    __ add(scratch,
           scratch,
           Operand(key_reg, LSL, kPointerSizeLog2 - kSmiTagSize));
    __ str(value_reg, MemOperand(scratch));
  } else {
    ASSERT(IsFastObjectElementsKind(elements_kind));
    __ add(scratch,
           elements_reg,
           Operand(FixedArray::kHeaderSize - kHeapObjectTag));
    STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
    __ add(scratch,
           scratch,
           Operand(key_reg, LSL, kPointerSizeLog2 - kSmiTagSize));
    __ str(value_reg, MemOperand(scratch));
    __ mov(receiver_reg, value_reg);
    __ RecordWrite(elements_reg,  // Object.
                   scratch,       // Address.
                   receiver_reg,  // Value.
                   kLRHasNotBeenSaved,
                   kDontSaveFPRegs);
  }
  // value_reg (r0) is preserved.
  // Done.
  __ Ret();

  __ bind(&miss_force_generic);
  TailCallBuiltin(masm, Builtins::kKeyedStoreIC_MissForceGeneric);

  __ bind(&transition_elements_kind);
  TailCallBuiltin(masm, Builtins::kKeyedStoreIC_Miss);

  if (is_js_array && IsGrowStoreMode(store_mode)) {
    // Grow the array by a single element if possible.
    __ bind(&grow);

    // Make sure the array is only growing by a single element, anything else
    // must be handled by the runtime. Flags already set by previous compare.
    __ b(ne, &miss_force_generic);

    // Check for the empty array, and preallocate a small backing store if
    // possible.
    __ ldr(length_reg,
           FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
    __ ldr(elements_reg,
           FieldMemOperand(receiver_reg, JSObject::kElementsOffset));
    __ CompareRoot(elements_reg, Heap::kEmptyFixedArrayRootIndex);
    __ b(ne, &check_capacity);

    int size = FixedArray::SizeFor(JSArray::kPreallocatedArrayElements);
    __ Allocate(size, elements_reg, scratch, scratch2, &slow, TAG_OBJECT);

    __ LoadRoot(scratch, Heap::kFixedArrayMapRootIndex);
    __ str(scratch, FieldMemOperand(elements_reg, JSObject::kMapOffset));
    __ mov(scratch, Operand(Smi::FromInt(JSArray::kPreallocatedArrayElements)));
    __ str(scratch, FieldMemOperand(elements_reg, FixedArray::kLengthOffset));
    __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
    for (int i = 1; i < JSArray::kPreallocatedArrayElements; ++i) {
      __ str(scratch, FieldMemOperand(elements_reg, FixedArray::SizeFor(i)));
    }

    // Store the element at index zero.
    __ str(value_reg, FieldMemOperand(elements_reg, FixedArray::SizeFor(0)));

    // Install the new backing store in the JSArray.
    __ str(elements_reg,
           FieldMemOperand(receiver_reg, JSObject::kElementsOffset));
    __ RecordWriteField(receiver_reg, JSObject::kElementsOffset, elements_reg,
                        scratch, kLRHasNotBeenSaved, kDontSaveFPRegs,
                        EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // Increment the length of the array.
    __ mov(length_reg, Operand(Smi::FromInt(1)));
    __ str(length_reg, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
    __ Ret();

    __ bind(&check_capacity);
    // Check for cow elements, in general they are not handled by this stub
    __ CheckMap(elements_reg,
                scratch,
                Heap::kFixedCOWArrayMapRootIndex,
                &miss_force_generic,
                DONT_DO_SMI_CHECK);

    __ ldr(scratch, FieldMemOperand(elements_reg, FixedArray::kLengthOffset));
    __ cmp(length_reg, scratch);
    __ b(hs, &slow);

    // Grow the array and finish the store.
    __ add(length_reg, length_reg, Operand(Smi::FromInt(1)));
    __ str(length_reg, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
    __ jmp(&finish_store);

    __ bind(&slow);
    TailCallBuiltin(masm, Builtins::kKeyedStoreIC_Slow);
  }
}


void KeyedStoreStubCompiler::GenerateStoreFastDoubleElement(
    MacroAssembler* masm,
    bool is_js_array,
    KeyedAccessStoreMode store_mode) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : key
  //  -- r2    : receiver
  //  -- lr    : return address
  //  -- r3    : scratch (elements backing store)
  //  -- r4    : scratch
  //  -- r5    : scratch
  // -----------------------------------
  Label miss_force_generic, transition_elements_kind, grow, slow;
  Label finish_store, check_capacity;

  Register value_reg = r0;
  Register key_reg = r1;
  Register receiver_reg = r2;
  Register elements_reg = r3;
  Register scratch1 = r4;
  Register scratch2 = r5;
  Register length_reg = r7;

  // This stub is meant to be tail-jumped to, the receiver must already
  // have been verified by the caller to not be a smi.

  // Check that the key is a smi or a heap number convertible to a smi.
  GenerateSmiKeyCheck(masm, key_reg, r4, r5, d1, d2, &miss_force_generic);

  __ ldr(elements_reg,
         FieldMemOperand(receiver_reg, JSObject::kElementsOffset));

  // Check that the key is within bounds.
  if (is_js_array) {
    __ ldr(scratch1, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
  } else {
    __ ldr(scratch1,
           FieldMemOperand(elements_reg, FixedArray::kLengthOffset));
  }
  // Compare smis, unsigned compare catches both negative and out-of-bound
  // indexes.
  __ cmp(key_reg, scratch1);
  if (IsGrowStoreMode(store_mode)) {
    __ b(hs, &grow);
  } else {
    __ b(hs, &miss_force_generic);
  }

  __ bind(&finish_store);
  __ StoreNumberToDoubleElements(value_reg, key_reg, elements_reg,
                                 scratch1, &transition_elements_kind);
  __ Ret();

  // Handle store cache miss, replacing the ic with the generic stub.
  __ bind(&miss_force_generic);
  TailCallBuiltin(masm, Builtins::kKeyedStoreIC_MissForceGeneric);

  __ bind(&transition_elements_kind);
  TailCallBuiltin(masm, Builtins::kKeyedStoreIC_Miss);

  if (is_js_array && IsGrowStoreMode(store_mode)) {
    // Grow the array by a single element if possible.
    __ bind(&grow);

    // Make sure the array is only growing by a single element, anything else
    // must be handled by the runtime. Flags already set by previous compare.
    __ b(ne, &miss_force_generic);

    // Transition on values that can't be stored in a FixedDoubleArray.
    Label value_is_smi;
    __ JumpIfSmi(value_reg, &value_is_smi);
    __ ldr(scratch1, FieldMemOperand(value_reg, HeapObject::kMapOffset));
    __ CompareRoot(scratch1, Heap::kHeapNumberMapRootIndex);
    __ b(ne, &transition_elements_kind);
    __ bind(&value_is_smi);

    // Check for the empty array, and preallocate a small backing store if
    // possible.
    __ ldr(length_reg,
           FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
    __ ldr(elements_reg,
           FieldMemOperand(receiver_reg, JSObject::kElementsOffset));
    __ CompareRoot(elements_reg, Heap::kEmptyFixedArrayRootIndex);
    __ b(ne, &check_capacity);

    int size = FixedDoubleArray::SizeFor(JSArray::kPreallocatedArrayElements);
    __ Allocate(size, elements_reg, scratch1, scratch2, &slow, TAG_OBJECT);

    // Initialize the new FixedDoubleArray.
    __ LoadRoot(scratch1, Heap::kFixedDoubleArrayMapRootIndex);
    __ str(scratch1, FieldMemOperand(elements_reg, JSObject::kMapOffset));
    __ mov(scratch1,
           Operand(Smi::FromInt(JSArray::kPreallocatedArrayElements)));
    __ str(scratch1,
           FieldMemOperand(elements_reg, FixedDoubleArray::kLengthOffset));

    __ mov(scratch1, elements_reg);
    __ StoreNumberToDoubleElements(value_reg, key_reg, scratch1,
                                   scratch2, &transition_elements_kind);

    __ mov(scratch1, Operand(kHoleNanLower32));
    __ mov(scratch2, Operand(kHoleNanUpper32));
    for (int i = 1; i < JSArray::kPreallocatedArrayElements; i++) {
      int offset = FixedDoubleArray::OffsetOfElementAt(i);
      __ str(scratch1, FieldMemOperand(elements_reg, offset));
      __ str(scratch2, FieldMemOperand(elements_reg, offset + kPointerSize));
    }

    // Install the new backing store in the JSArray.
    __ str(elements_reg,
           FieldMemOperand(receiver_reg, JSObject::kElementsOffset));
    __ RecordWriteField(receiver_reg, JSObject::kElementsOffset, elements_reg,
                        scratch1, kLRHasNotBeenSaved, kDontSaveFPRegs,
                        EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    // Increment the length of the array.
    __ mov(length_reg, Operand(Smi::FromInt(1)));
    __ str(length_reg, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
    __ ldr(elements_reg,
           FieldMemOperand(receiver_reg, JSObject::kElementsOffset));
    __ Ret();

    __ bind(&check_capacity);
    // Make sure that the backing store can hold additional elements.
    __ ldr(scratch1,
           FieldMemOperand(elements_reg, FixedDoubleArray::kLengthOffset));
    __ cmp(length_reg, scratch1);
    __ b(hs, &slow);

    // Grow the array and finish the store.
    __ add(length_reg, length_reg, Operand(Smi::FromInt(1)));
    __ str(length_reg, FieldMemOperand(receiver_reg, JSArray::kLengthOffset));
    __ jmp(&finish_store);

    __ bind(&slow);
    TailCallBuiltin(masm, Builtins::kKeyedStoreIC_Slow);
  }
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
