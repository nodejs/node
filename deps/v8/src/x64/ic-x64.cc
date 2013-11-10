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

#if V8_TARGET_ARCH_X64

#include "codegen.h"
#include "ic-inl.h"
#include "runtime.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Static IC stub generators.
//

#define __ ACCESS_MASM(masm)


static void GenerateGlobalInstanceTypeCheck(MacroAssembler* masm,
                                            Register type,
                                            Label* global_object) {
  // Register usage:
  //   type: holds the receiver instance type on entry.
  __ cmpb(type, Immediate(JS_GLOBAL_OBJECT_TYPE));
  __ j(equal, global_object);
  __ cmpb(type, Immediate(JS_BUILTINS_OBJECT_TYPE));
  __ j(equal, global_object);
  __ cmpb(type, Immediate(JS_GLOBAL_PROXY_TYPE));
  __ j(equal, global_object);
}


// Generated code falls through if the receiver is a regular non-global
// JS object with slow properties and no interceptors.
static void GenerateNameDictionaryReceiverCheck(MacroAssembler* masm,
                                                Register receiver,
                                                Register r0,
                                                Register r1,
                                                Label* miss) {
  // Register usage:
  //   receiver: holds the receiver on entry and is unchanged.
  //   r0: used to hold receiver instance type.
  //       Holds the property dictionary on fall through.
  //   r1: used to hold receivers map.

  __ JumpIfSmi(receiver, miss);

  // Check that the receiver is a valid JS object.
  __ movq(r1, FieldOperand(receiver, HeapObject::kMapOffset));
  __ movb(r0, FieldOperand(r1, Map::kInstanceTypeOffset));
  __ cmpb(r0, Immediate(FIRST_SPEC_OBJECT_TYPE));
  __ j(below, miss);

  // If this assert fails, we have to check upper bound too.
  STATIC_ASSERT(LAST_TYPE == LAST_SPEC_OBJECT_TYPE);

  GenerateGlobalInstanceTypeCheck(masm, r0, miss);

  // Check for non-global object that requires access check.
  __ testb(FieldOperand(r1, Map::kBitFieldOffset),
           Immediate((1 << Map::kIsAccessCheckNeeded) |
                     (1 << Map::kHasNamedInterceptor)));
  __ j(not_zero, miss);

  __ movq(r0, FieldOperand(receiver, JSObject::kPropertiesOffset));
  __ CompareRoot(FieldOperand(r0, HeapObject::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(not_equal, miss);
}



// Helper function used to load a property from a dictionary backing storage.
// This function may return false negatives, so miss_label
// must always call a backup property load that is complete.
// This function is safe to call if name is not an internalized string,
// and will jump to the miss_label in that case.
// The generated code assumes that the receiver has slow properties,
// is not a global object and does not have interceptors.
static void GenerateDictionaryLoad(MacroAssembler* masm,
                                   Label* miss_label,
                                   Register elements,
                                   Register name,
                                   Register r0,
                                   Register r1,
                                   Register result) {
  // Register use:
  //
  // elements - holds the property dictionary on entry and is unchanged.
  //
  // name - holds the name of the property on entry and is unchanged.
  //
  // r0   - used to hold the capacity of the property dictionary.
  //
  // r1   - used to hold the index into the property dictionary.
  //
  // result - holds the result on exit if the load succeeded.

  Label done;

  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm,
                                                   miss_label,
                                                   &done,
                                                   elements,
                                                   name,
                                                   r0,
                                                   r1);

  // If probing finds an entry in the dictionary, r1 contains the
  // index into the dictionary. Check that the value is a normal
  // property.
  __ bind(&done);
  const int kElementsStartOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;
  const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  __ Test(Operand(elements, r1, times_pointer_size,
                  kDetailsOffset - kHeapObjectTag),
          Smi::FromInt(PropertyDetails::TypeField::kMask));
  __ j(not_zero, miss_label);

  // Get the value at the masked, scaled index.
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ movq(result,
          Operand(elements, r1, times_pointer_size,
                  kValueOffset - kHeapObjectTag));
}


// Helper function used to store a property to a dictionary backing
// storage. This function may fail to store a property even though it
// is in the dictionary, so code at miss_label must always call a
// backup property store that is complete. This function is safe to
// call if name is not an internalized string, and will jump to the miss_label
// in that case. The generated code assumes that the receiver has slow
// properties, is not a global object and does not have interceptors.
static void GenerateDictionaryStore(MacroAssembler* masm,
                                    Label* miss_label,
                                    Register elements,
                                    Register name,
                                    Register value,
                                    Register scratch0,
                                    Register scratch1) {
  // Register use:
  //
  // elements - holds the property dictionary on entry and is clobbered.
  //
  // name - holds the name of the property on entry and is unchanged.
  //
  // value - holds the value to store and is unchanged.
  //
  // scratch0 - used during the positive dictionary lookup and is clobbered.
  //
  // scratch1 - used for index into the property dictionary and is clobbered.
  Label done;

  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm,
                                                   miss_label,
                                                   &done,
                                                   elements,
                                                   name,
                                                   scratch0,
                                                   scratch1);

  // If probing finds an entry in the dictionary, scratch0 contains the
  // index into the dictionary. Check that the value is a normal
  // property that is not read only.
  __ bind(&done);
  const int kElementsStartOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;
  const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  const int kTypeAndReadOnlyMask =
      (PropertyDetails::TypeField::kMask |
       PropertyDetails::AttributesField::encode(READ_ONLY)) << kSmiTagSize;
  __ Test(Operand(elements,
                  scratch1,
                  times_pointer_size,
                  kDetailsOffset - kHeapObjectTag),
          Smi::FromInt(kTypeAndReadOnlyMask));
  __ j(not_zero, miss_label);

  // Store the value at the masked, scaled index.
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ lea(scratch1, Operand(elements,
                           scratch1,
                           times_pointer_size,
                           kValueOffset - kHeapObjectTag));
  __ movq(Operand(scratch1, 0), value);

  // Update write barrier. Make sure not to clobber the value.
  __ movq(scratch0, value);
  __ RecordWrite(elements, scratch1, scratch0, kDontSaveFPRegs);
}


// Checks the receiver for special cases (value type, slow case bits).
// Falls through for regular JS object.
static void GenerateKeyedLoadReceiverCheck(MacroAssembler* masm,
                                           Register receiver,
                                           Register map,
                                           int interceptor_bit,
                                           Label* slow) {
  // Register use:
  //   receiver - holds the receiver and is unchanged.
  // Scratch registers:
  //   map - used to hold the map of the receiver.

  // Check that the object isn't a smi.
  __ JumpIfSmi(receiver, slow);

  // Check that the object is some kind of JS object EXCEPT JS Value type.
  // In the case that the object is a value-wrapper object,
  // we enter the runtime system to make sure that indexing
  // into string objects work as intended.
  ASSERT(JS_OBJECT_TYPE > JS_VALUE_TYPE);
  __ CmpObjectType(receiver, JS_OBJECT_TYPE, map);
  __ j(below, slow);

  // Check bit field.
  __ testb(FieldOperand(map, Map::kBitFieldOffset),
           Immediate((1 << Map::kIsAccessCheckNeeded) |
                     (1 << interceptor_bit)));
  __ j(not_zero, slow);
}


// Loads an indexed element from a fast case array.
// If not_fast_array is NULL, doesn't perform the elements map check.
static void GenerateFastArrayLoad(MacroAssembler* masm,
                                  Register receiver,
                                  Register key,
                                  Register elements,
                                  Register scratch,
                                  Register result,
                                  Label* not_fast_array,
                                  Label* out_of_range) {
  // Register use:
  //
  // receiver - holds the receiver on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // key      - holds the smi key on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // elements - holds the elements of the receiver on exit.
  //
  // result   - holds the result on exit if the load succeeded.
  //            Allowed to be the the same as 'receiver' or 'key'.
  //            Unchanged on bailout so 'receiver' and 'key' can be safely
  //            used by further computation.
  //
  // Scratch registers:
  //
  //   scratch - used to hold elements of the receiver and the loaded value.

  __ movq(elements, FieldOperand(receiver, JSObject::kElementsOffset));
  if (not_fast_array != NULL) {
    // Check that the object is in fast mode and writable.
    __ CompareRoot(FieldOperand(elements, HeapObject::kMapOffset),
                   Heap::kFixedArrayMapRootIndex);
    __ j(not_equal, not_fast_array);
  } else {
    __ AssertFastElements(elements);
  }
  // Check that the key (index) is within bounds.
  __ SmiCompare(key, FieldOperand(elements, FixedArray::kLengthOffset));
  // Unsigned comparison rejects negative indices.
  __ j(above_equal, out_of_range);
  // Fast case: Do the load.
  SmiIndex index = masm->SmiToIndex(scratch, key, kPointerSizeLog2);
  __ movq(scratch, FieldOperand(elements,
                                index.reg,
                                index.scale,
                                FixedArray::kHeaderSize));
  __ CompareRoot(scratch, Heap::kTheHoleValueRootIndex);
  // In case the loaded value is the_hole we have to consult GetProperty
  // to ensure the prototype chain is searched.
  __ j(equal, out_of_range);
  if (!result.is(scratch)) {
    __ movq(result, scratch);
  }
}


// Checks whether a key is an array index string or a unique name.
// Falls through if the key is a unique name.
static void GenerateKeyNameCheck(MacroAssembler* masm,
                                 Register key,
                                 Register map,
                                 Register hash,
                                 Label* index_string,
                                 Label* not_unique) {
  // Register use:
  //   key - holds the key and is unchanged. Assumed to be non-smi.
  // Scratch registers:
  //   map - used to hold the map of the key.
  //   hash - used to hold the hash of the key.
  Label unique;
  __ CmpObjectType(key, LAST_UNIQUE_NAME_TYPE, map);
  __ j(above, not_unique);
  STATIC_ASSERT(LAST_UNIQUE_NAME_TYPE == FIRST_NONSTRING_TYPE);
  __ j(equal, &unique);

  // Is the string an array index, with cached numeric value?
  __ movl(hash, FieldOperand(key, Name::kHashFieldOffset));
  __ testl(hash, Immediate(Name::kContainsCachedArrayIndexMask));
  __ j(zero, index_string);  // The value in hash is used at jump target.

  // Is the string internalized? We already know it's a string so a single
  // bit test is enough.
  STATIC_ASSERT(kNotInternalizedTag != 0);
  __ testb(FieldOperand(map, Map::kInstanceTypeOffset),
           Immediate(kIsNotInternalizedMask));
  __ j(not_zero, not_unique);

  __ bind(&unique);
}



void KeyedLoadIC::GenerateGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label slow, check_name, index_smi, index_name, property_array_property;
  Label probe_dictionary, check_number_dictionary;

  // Check that the key is a smi.
  __ JumpIfNotSmi(rax, &check_name);
  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from below
  // where a numeric string is converted to a smi.

  GenerateKeyedLoadReceiverCheck(
      masm, rdx, rcx, Map::kHasIndexedInterceptor, &slow);

  // Check the receiver's map to see if it has fast elements.
  __ CheckFastElements(rcx, &check_number_dictionary);

  GenerateFastArrayLoad(masm,
                        rdx,
                        rax,
                        rcx,
                        rbx,
                        rax,
                        NULL,
                        &slow);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->keyed_load_generic_smi(), 1);
  __ ret(0);

  __ bind(&check_number_dictionary);
  __ SmiToInteger32(rbx, rax);
  __ movq(rcx, FieldOperand(rdx, JSObject::kElementsOffset));

  // Check whether the elements is a number dictionary.
  // rdx: receiver
  // rax: key
  // rbx: key as untagged int32
  // rcx: elements
  __ CompareRoot(FieldOperand(rcx, HeapObject::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(not_equal, &slow);
  __ LoadFromNumberDictionary(&slow, rcx, rax, rbx, r9, rdi, rax);
  __ ret(0);

  __ bind(&slow);
  // Slow case: Jump to runtime.
  // rdx: receiver
  // rax: key
  __ IncrementCounter(counters->keyed_load_generic_slow(), 1);
  GenerateRuntimeGetProperty(masm);

  __ bind(&check_name);
  GenerateKeyNameCheck(masm, rax, rcx, rbx, &index_name, &slow);

  GenerateKeyedLoadReceiverCheck(
      masm, rdx, rcx, Map::kHasNamedInterceptor, &slow);

  // If the receiver is a fast-case object, check the keyed lookup
  // cache. Otherwise probe the dictionary leaving result in rcx.
  __ movq(rbx, FieldOperand(rdx, JSObject::kPropertiesOffset));
  __ CompareRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(equal, &probe_dictionary);

  // Load the map of the receiver, compute the keyed lookup cache hash
  // based on 32 bits of the map pointer and the string hash.
  __ movq(rbx, FieldOperand(rdx, HeapObject::kMapOffset));
  __ movl(rcx, rbx);
  __ shr(rcx, Immediate(KeyedLookupCache::kMapHashShift));
  __ movl(rdi, FieldOperand(rax, String::kHashFieldOffset));
  __ shr(rdi, Immediate(String::kHashShift));
  __ xor_(rcx, rdi);
  int mask = (KeyedLookupCache::kCapacityMask & KeyedLookupCache::kHashMask);
  __ and_(rcx, Immediate(mask));

  // Load the key (consisting of map and internalized string) from the cache and
  // check for match.
  Label load_in_object_property;
  static const int kEntriesPerBucket = KeyedLookupCache::kEntriesPerBucket;
  Label hit_on_nth_entry[kEntriesPerBucket];
  ExternalReference cache_keys
      = ExternalReference::keyed_lookup_cache_keys(masm->isolate());

  for (int i = 0; i < kEntriesPerBucket - 1; i++) {
    Label try_next_entry;
    __ movq(rdi, rcx);
    __ shl(rdi, Immediate(kPointerSizeLog2 + 1));
    __ LoadAddress(kScratchRegister, cache_keys);
    int off = kPointerSize * i * 2;
    __ cmpq(rbx, Operand(kScratchRegister, rdi, times_1, off));
    __ j(not_equal, &try_next_entry);
    __ cmpq(rax, Operand(kScratchRegister, rdi, times_1, off + kPointerSize));
    __ j(equal, &hit_on_nth_entry[i]);
    __ bind(&try_next_entry);
  }

  int off = kPointerSize * (kEntriesPerBucket - 1) * 2;
  __ cmpq(rbx, Operand(kScratchRegister, rdi, times_1, off));
  __ j(not_equal, &slow);
  __ cmpq(rax, Operand(kScratchRegister, rdi, times_1, off + kPointerSize));
  __ j(not_equal, &slow);

  // Get field offset, which is a 32-bit integer.
  ExternalReference cache_field_offsets
      = ExternalReference::keyed_lookup_cache_field_offsets(masm->isolate());

  // Hit on nth entry.
  for (int i = kEntriesPerBucket - 1; i >= 0; i--) {
    __ bind(&hit_on_nth_entry[i]);
    if (i != 0) {
      __ addl(rcx, Immediate(i));
    }
    __ LoadAddress(kScratchRegister, cache_field_offsets);
    __ movl(rdi, Operand(kScratchRegister, rcx, times_4, 0));
    __ movzxbq(rcx, FieldOperand(rbx, Map::kInObjectPropertiesOffset));
    __ subq(rdi, rcx);
    __ j(above_equal, &property_array_property);
    if (i != 0) {
      __ jmp(&load_in_object_property);
    }
  }

  // Load in-object property.
  __ bind(&load_in_object_property);
  __ movzxbq(rcx, FieldOperand(rbx, Map::kInstanceSizeOffset));
  __ addq(rcx, rdi);
  __ movq(rax, FieldOperand(rdx, rcx, times_pointer_size, 0));
  __ IncrementCounter(counters->keyed_load_generic_lookup_cache(), 1);
  __ ret(0);

  // Load property array property.
  __ bind(&property_array_property);
  __ movq(rax, FieldOperand(rdx, JSObject::kPropertiesOffset));
  __ movq(rax, FieldOperand(rax, rdi, times_pointer_size,
                            FixedArray::kHeaderSize));
  __ IncrementCounter(counters->keyed_load_generic_lookup_cache(), 1);
  __ ret(0);

  // Do a quick inline probe of the receiver's dictionary, if it
  // exists.
  __ bind(&probe_dictionary);
  // rdx: receiver
  // rax: key
  // rbx: elements

  __ movq(rcx, FieldOperand(rdx, JSObject::kMapOffset));
  __ movb(rcx, FieldOperand(rcx, Map::kInstanceTypeOffset));
  GenerateGlobalInstanceTypeCheck(masm, rcx, &slow);

  GenerateDictionaryLoad(masm, &slow, rbx, rax, rcx, rdi, rax);
  __ IncrementCounter(counters->keyed_load_generic_symbol(), 1);
  __ ret(0);

  __ bind(&index_name);
  __ IndexFromHash(rbx, rax);
  __ jmp(&index_smi);
}


void KeyedLoadIC::GenerateString(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  Register receiver = rdx;
  Register index = rax;
  Register scratch = rcx;
  Register result = rax;

  StringCharAtGenerator char_at_generator(receiver,
                                          index,
                                          scratch,
                                          result,
                                          &miss,  // When not a string.
                                          &miss,  // When not a number.
                                          &miss,  // When index out of range.
                                          STRING_INDEX_IS_ARRAY_INDEX);
  char_at_generator.GenerateFast(masm);
  __ ret(0);

  StubRuntimeCallHelper call_helper;
  char_at_generator.GenerateSlow(masm, call_helper);

  __ bind(&miss);
  GenerateMiss(masm, MISS);
}


void KeyedLoadIC::GenerateIndexedInterceptor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label slow;

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(rdx, &slow);

  // Check that the key is an array index, that is Uint32.
  STATIC_ASSERT(kSmiValueSize <= 32);
  __ JumpUnlessNonNegativeSmi(rax, &slow);

  // Get the map of the receiver.
  __ movq(rcx, FieldOperand(rdx, HeapObject::kMapOffset));

  // Check that it has indexed interceptor and access checks
  // are not enabled for this object.
  __ movb(rcx, FieldOperand(rcx, Map::kBitFieldOffset));
  __ andb(rcx, Immediate(kSlowCaseBitFieldMask));
  __ cmpb(rcx, Immediate(1 << Map::kHasIndexedInterceptor));
  __ j(not_zero, &slow);

  // Everything is fine, call runtime.
  __ PopReturnAddressTo(rcx);
  __ push(rdx);  // receiver
  __ push(rax);  // key
  __ PushReturnAddressFrom(rcx);

  // Perform tail call to the entry.
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(kKeyedLoadPropertyWithInterceptor),
                        masm->isolate()),
      2,
      1);

  __ bind(&slow);
  GenerateMiss(masm, MISS);
}


static void KeyedStoreGenerateGenericHelper(
    MacroAssembler* masm,
    Label* fast_object,
    Label* fast_double,
    Label* slow,
    KeyedStoreCheckMap check_map,
    KeyedStoreIncrementLength increment_length) {
  Label transition_smi_elements;
  Label finish_object_store, non_double_value, transition_double_elements;
  Label fast_double_without_map_check;
  // Fast case: Do the store, could be either Object or double.
  __ bind(fast_object);
  // rax: value
  // rbx: receiver's elements array (a FixedArray)
  // rcx: index
  // rdx: receiver (a JSArray)
  // r9: map of receiver
  if (check_map == kCheckMap) {
    __ movq(rdi, FieldOperand(rbx, HeapObject::kMapOffset));
    __ CompareRoot(rdi, Heap::kFixedArrayMapRootIndex);
    __ j(not_equal, fast_double);
  }
  // Smi stores don't require further checks.
  Label non_smi_value;
  __ JumpIfNotSmi(rax, &non_smi_value);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ leal(rdi, Operand(rcx, 1));
    __ Integer32ToSmiField(FieldOperand(rdx, JSArray::kLengthOffset), rdi);
  }
  // It's irrelevant whether array is smi-only or not when writing a smi.
  __ movq(FieldOperand(rbx, rcx, times_pointer_size, FixedArray::kHeaderSize),
          rax);
  __ ret(0);

  __ bind(&non_smi_value);
  // Writing a non-smi, check whether array allows non-smi elements.
  // r9: receiver's map
  __ CheckFastObjectElements(r9, &transition_smi_elements);

  __ bind(&finish_object_store);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ leal(rdi, Operand(rcx, 1));
    __ Integer32ToSmiField(FieldOperand(rdx, JSArray::kLengthOffset), rdi);
  }
  __ movq(FieldOperand(rbx, rcx, times_pointer_size, FixedArray::kHeaderSize),
          rax);
  __ movq(rdx, rax);  // Preserve the value which is returned.
  __ RecordWriteArray(
      rbx, rdx, rcx, kDontSaveFPRegs, EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  __ ret(0);

  __ bind(fast_double);
  if (check_map == kCheckMap) {
    // Check for fast double array case. If this fails, call through to the
    // runtime.
    // rdi: elements array's map
    __ CompareRoot(rdi, Heap::kFixedDoubleArrayMapRootIndex);
    __ j(not_equal, slow);
  }
  __ bind(&fast_double_without_map_check);
  __ StoreNumberToDoubleElements(rax, rbx, rcx, xmm0,
                                 &transition_double_elements);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ leal(rdi, Operand(rcx, 1));
    __ Integer32ToSmiField(FieldOperand(rdx, JSArray::kLengthOffset), rdi);
  }
  __ ret(0);

  __ bind(&transition_smi_elements);
  __ movq(rbx, FieldOperand(rdx, HeapObject::kMapOffset));

  // Transition the array appropriately depending on the value type.
  __ movq(r9, FieldOperand(rax, HeapObject::kMapOffset));
  __ CompareRoot(r9, Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, &non_double_value);

  // Value is a double. Transition FAST_SMI_ELEMENTS ->
  // FAST_DOUBLE_ELEMENTS and complete the store.
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                         FAST_DOUBLE_ELEMENTS,
                                         rbx,
                                         rdi,
                                         slow);
  AllocationSiteMode mode = AllocationSite::GetMode(FAST_SMI_ELEMENTS,
                                                    FAST_DOUBLE_ELEMENTS);
  ElementsTransitionGenerator::GenerateSmiToDouble(masm, mode, slow);
  __ movq(rbx, FieldOperand(rdx, JSObject::kElementsOffset));
  __ jmp(&fast_double_without_map_check);

  __ bind(&non_double_value);
  // Value is not a double, FAST_SMI_ELEMENTS -> FAST_ELEMENTS
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                         FAST_ELEMENTS,
                                         rbx,
                                         rdi,
                                         slow);
  mode = AllocationSite::GetMode(FAST_SMI_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateMapChangeElementsTransition(masm, mode,
                                                                   slow);
  __ movq(rbx, FieldOperand(rdx, JSObject::kElementsOffset));
  __ jmp(&finish_object_store);

  __ bind(&transition_double_elements);
  // Elements are FAST_DOUBLE_ELEMENTS, but value is an Object that's not a
  // HeapNumber. Make sure that the receiver is a Array with FAST_ELEMENTS and
  // transition array from FAST_DOUBLE_ELEMENTS to FAST_ELEMENTS
  __ movq(rbx, FieldOperand(rdx, HeapObject::kMapOffset));
  __ LoadTransitionedArrayMapConditional(FAST_DOUBLE_ELEMENTS,
                                         FAST_ELEMENTS,
                                         rbx,
                                         rdi,
                                         slow);
  mode = AllocationSite::GetMode(FAST_DOUBLE_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateDoubleToObject(masm, mode, slow);
  __ movq(rbx, FieldOperand(rdx, JSObject::kElementsOffset));
  __ jmp(&finish_object_store);
}


void KeyedStoreIC::GenerateGeneric(MacroAssembler* masm,
                                   StrictModeFlag strict_mode) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label slow, slow_with_tagged_index, fast_object, fast_object_grow;
  Label fast_double, fast_double_grow;
  Label array, extra, check_if_double_array;

  // Check that the object isn't a smi.
  __ JumpIfSmi(rdx, &slow_with_tagged_index);
  // Get the map from the receiver.
  __ movq(r9, FieldOperand(rdx, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.  We need
  // to do this because this generic stub does not perform map checks.
  __ testb(FieldOperand(r9, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &slow_with_tagged_index);
  // Check that the key is a smi.
  __ JumpIfNotSmi(rcx, &slow_with_tagged_index);
  __ SmiToInteger32(rcx, rcx);

  __ CmpInstanceType(r9, JS_ARRAY_TYPE);
  __ j(equal, &array);
  // Check that the object is some kind of JSObject.
  __ CmpInstanceType(r9, FIRST_JS_OBJECT_TYPE);
  __ j(below, &slow);

  // Object case: Check key against length in the elements array.
  // rax: value
  // rdx: JSObject
  // rcx: index
  __ movq(rbx, FieldOperand(rdx, JSObject::kElementsOffset));
  // Check array bounds.
  __ SmiCompareInteger32(FieldOperand(rbx, FixedArray::kLengthOffset), rcx);
  // rax: value
  // rbx: FixedArray
  // rcx: index
  __ j(above, &fast_object);

  // Slow case: call runtime.
  __ bind(&slow);
  __ Integer32ToSmi(rcx, rcx);
  __ bind(&slow_with_tagged_index);
  GenerateRuntimeSetProperty(masm, strict_mode);
  // Never returns to here.

  // Extra capacity case: Check if there is extra capacity to
  // perform the store and update the length. Used for adding one
  // element to the array by writing to array[array.length].
  __ bind(&extra);
  // rax: value
  // rdx: receiver (a JSArray)
  // rbx: receiver's elements array (a FixedArray)
  // rcx: index
  // flags: smicompare (rdx.length(), rbx)
  __ j(not_equal, &slow);  // do not leave holes in the array
  __ SmiCompareInteger32(FieldOperand(rbx, FixedArray::kLengthOffset), rcx);
  __ j(below_equal, &slow);
  // Increment index to get new length.
  __ movq(rdi, FieldOperand(rbx, HeapObject::kMapOffset));
  __ CompareRoot(rdi, Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &check_if_double_array);
  __ jmp(&fast_object_grow);

  __ bind(&check_if_double_array);
  // rdi: elements array's map
  __ CompareRoot(rdi, Heap::kFixedDoubleArrayMapRootIndex);
  __ j(not_equal, &slow);
  __ jmp(&fast_double_grow);

  // Array case: Get the length and the elements array from the JS
  // array. Check that the array is in fast mode (and writable); if it
  // is the length is always a smi.
  __ bind(&array);
  // rax: value
  // rdx: receiver (a JSArray)
  // rcx: index
  __ movq(rbx, FieldOperand(rdx, JSObject::kElementsOffset));

  // Check the key against the length in the array, compute the
  // address to store into and fall through to fast case.
  __ SmiCompareInteger32(FieldOperand(rdx, JSArray::kLengthOffset), rcx);
  __ j(below_equal, &extra);

  KeyedStoreGenerateGenericHelper(masm, &fast_object, &fast_double,
                                  &slow, kCheckMap, kDontIncrementLength);
  KeyedStoreGenerateGenericHelper(masm, &fast_object_grow, &fast_double_grow,
                                  &slow, kDontCheckMap, kIncrementLength);
}


// The generated code does not accept smi keys.
// The generated code falls through if both probes miss.
void CallICBase::GenerateMonomorphicCacheProbe(MacroAssembler* masm,
                                               int argc,
                                               Code::Kind kind,
                                               Code::ExtraICState extra_state) {
  // ----------- S t a t e -------------
  // rcx                      : function name
  // rdx                      : receiver
  // -----------------------------------
  Label number, non_number, non_string, boolean, probe, miss;

  // Probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(kind,
                                         MONOMORPHIC,
                                         extra_state,
                                         Code::NORMAL,
                                         argc);
  masm->isolate()->stub_cache()->GenerateProbe(
      masm, flags, rdx, rcx, rbx, rax);

  // If the stub cache probing failed, the receiver might be a value.
  // For value objects, we use the map of the prototype objects for
  // the corresponding JSValue for the cache and that is what we need
  // to probe.
  //
  // Check for number.
  __ JumpIfSmi(rdx, &number);
  __ CmpObjectType(rdx, HEAP_NUMBER_TYPE, rbx);
  __ j(not_equal, &non_number);
  __ bind(&number);
  StubCompiler::GenerateLoadGlobalFunctionPrototype(
      masm, Context::NUMBER_FUNCTION_INDEX, rdx);
  __ jmp(&probe);

  // Check for string.
  __ bind(&non_number);
  __ CmpInstanceType(rbx, FIRST_NONSTRING_TYPE);
  __ j(above_equal, &non_string);
  StubCompiler::GenerateLoadGlobalFunctionPrototype(
      masm, Context::STRING_FUNCTION_INDEX, rdx);
  __ jmp(&probe);

  // Check for boolean.
  __ bind(&non_string);
  __ CompareRoot(rdx, Heap::kTrueValueRootIndex);
  __ j(equal, &boolean);
  __ CompareRoot(rdx, Heap::kFalseValueRootIndex);
  __ j(not_equal, &miss);
  __ bind(&boolean);
  StubCompiler::GenerateLoadGlobalFunctionPrototype(
      masm, Context::BOOLEAN_FUNCTION_INDEX, rdx);

  // Probe the stub cache for the value object.
  __ bind(&probe);
  masm->isolate()->stub_cache()->GenerateProbe(
      masm, flags, rdx, rcx, rbx, no_reg);

  __ bind(&miss);
}


static void GenerateFunctionTailCall(MacroAssembler* masm,
                                     int argc,
                                     Label* miss) {
  // ----------- S t a t e -------------
  // rcx                 : function name
  // rdi                 : function
  // rsp[0]              : return address
  // rsp[8]              : argument argc
  // rsp[16]             : argument argc - 1
  // ...
  // rsp[argc * 8]       : argument 1
  // rsp[(argc + 1) * 8] : argument 0 = receiver
  // -----------------------------------
  __ JumpIfSmi(rdi, miss);
  // Check that the value is a JavaScript function.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rdx);
  __ j(not_equal, miss);

  // Invoke the function.
  ParameterCount actual(argc);
  __ InvokeFunction(rdi, actual, JUMP_FUNCTION,
                    NullCallWrapper(), CALL_AS_METHOD);
}


// The generated code falls through if the call should be handled by runtime.
void CallICBase::GenerateNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // rcx                 : function name
  // rsp[0]              : return address
  // rsp[8]              : argument argc
  // rsp[16]             : argument argc - 1
  // ...
  // rsp[argc * 8]       : argument 1
  // rsp[(argc + 1) * 8] : argument 0 = receiver
  // -----------------------------------
  Label miss;

  StackArgumentsAccessor args(rsp, argc);
  __ movq(rdx, args.GetReceiverOperand());

  GenerateNameDictionaryReceiverCheck(masm, rdx, rax, rbx, &miss);

  // rax: elements
  // Search the dictionary placing the result in rdi.
  GenerateDictionaryLoad(masm, &miss, rax, rcx, rbx, rdi, rdi);

  GenerateFunctionTailCall(masm, argc, &miss);

  __ bind(&miss);
}


void CallICBase::GenerateMiss(MacroAssembler* masm,
                              int argc,
                              IC::UtilityId id,
                              Code::ExtraICState extra_state) {
  // ----------- S t a t e -------------
  // rcx                 : function name
  // rsp[0]              : return address
  // rsp[8]              : argument argc
  // rsp[16]             : argument argc - 1
  // ...
  // rsp[argc * 8]       : argument 1
  // rsp[(argc + 1) * 8] : argument 0 = receiver
  // -----------------------------------

  Counters* counters = masm->isolate()->counters();
  if (id == IC::kCallIC_Miss) {
    __ IncrementCounter(counters->call_miss(), 1);
  } else {
    __ IncrementCounter(counters->keyed_call_miss(), 1);
  }

  StackArgumentsAccessor args(rsp, argc);
  __ movq(rdx, args.GetReceiverOperand());

  // Enter an internal frame.
  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Push the receiver and the name of the function.
    __ push(rdx);
    __ push(rcx);

    // Call the entry.
    CEntryStub stub(1);
    __ Set(rax, 2);
    __ LoadAddress(rbx, ExternalReference(IC_Utility(id), masm->isolate()));
    __ CallStub(&stub);

    // Move result to rdi and exit the internal frame.
    __ movq(rdi, rax);
  }

  // Check if the receiver is a global object of some sort.
  // This can happen only for regular CallIC but not KeyedCallIC.
  if (id == IC::kCallIC_Miss) {
    Label invoke, global;
    __ movq(rdx, args.GetReceiverOperand());
    __ JumpIfSmi(rdx, &invoke);
    __ CmpObjectType(rdx, JS_GLOBAL_OBJECT_TYPE, rcx);
    __ j(equal, &global);
    __ CmpInstanceType(rcx, JS_BUILTINS_OBJECT_TYPE);
    __ j(not_equal, &invoke);

    // Patch the receiver on the stack.
    __ bind(&global);
    __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalReceiverOffset));
    __ movq(args.GetReceiverOperand(), rdx);
    __ bind(&invoke);
  }

  // Invoke the function.
  CallKind call_kind = CallICBase::Contextual::decode(extra_state)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  ParameterCount actual(argc);
  __ InvokeFunction(rdi,
                    actual,
                    JUMP_FUNCTION,
                    NullCallWrapper(),
                    call_kind);
}


void CallIC::GenerateMegamorphic(MacroAssembler* masm,
                                 int argc,
                                 Code::ExtraICState extra_ic_state) {
  // ----------- S t a t e -------------
  // rcx                 : function name
  // rsp[0]              : return address
  // rsp[8]              : argument argc
  // rsp[16]             : argument argc - 1
  // ...
  // rsp[argc * 8]       : argument 1
  // rsp[(argc + 1) * 8] : argument 0 = receiver
  // -----------------------------------

  StackArgumentsAccessor args(rsp, argc);
  __ movq(rdx, args.GetReceiverOperand());
  GenerateMonomorphicCacheProbe(masm, argc, Code::CALL_IC, extra_ic_state);
  GenerateMiss(masm, argc, extra_ic_state);
}


void KeyedCallIC::GenerateMegamorphic(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // rcx                 : function name
  // rsp[0]              : return address
  // rsp[8]              : argument argc
  // rsp[16]             : argument argc - 1
  // ...
  // rsp[argc * 8]       : argument 1
  // rsp[(argc + 1) * 8] : argument 0 = receiver
  // -----------------------------------

  StackArgumentsAccessor args(rsp, argc);
  __ movq(rdx, args.GetReceiverOperand());

  Label do_call, slow_call, slow_load;
  Label check_number_dictionary, check_name, lookup_monomorphic_cache;
  Label index_smi, index_name;

  // Check that the key is a smi.
  __ JumpIfNotSmi(rcx, &check_name);

  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from below
  // where a numeric string is converted to a smi.

  GenerateKeyedLoadReceiverCheck(
      masm, rdx, rax, Map::kHasIndexedInterceptor, &slow_call);

  GenerateFastArrayLoad(
      masm, rdx, rcx, rax, rbx, rdi, &check_number_dictionary, &slow_load);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->keyed_call_generic_smi_fast(), 1);

  __ bind(&do_call);
  // receiver in rdx is not used after this point.
  // rcx: key
  // rdi: function
  GenerateFunctionTailCall(masm, argc, &slow_call);

  __ bind(&check_number_dictionary);
  // rax: elements
  // rcx: smi key
  // Check whether the elements is a number dictionary.
  __ CompareRoot(FieldOperand(rax, HeapObject::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(not_equal, &slow_load);
  __ SmiToInteger32(rbx, rcx);
  // ebx: untagged index
  __ LoadFromNumberDictionary(&slow_load, rax, rcx, rbx, r9, rdi, rdi);
  __ IncrementCounter(counters->keyed_call_generic_smi_dict(), 1);
  __ jmp(&do_call);

  __ bind(&slow_load);
  // This branch is taken when calling KeyedCallIC_Miss is neither required
  // nor beneficial.
  __ IncrementCounter(counters->keyed_call_generic_slow_load(), 1);
  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(rcx);  // save the key
    __ push(rdx);  // pass the receiver
    __ push(rcx);  // pass the key
    __ CallRuntime(Runtime::kKeyedGetProperty, 2);
    __ pop(rcx);  // restore the key
  }
  __ movq(rdi, rax);
  __ jmp(&do_call);

  __ bind(&check_name);
  GenerateKeyNameCheck(masm, rcx, rax, rbx, &index_name, &slow_call);

  // The key is known to be a unique name.
  // If the receiver is a regular JS object with slow properties then do
  // a quick inline probe of the receiver's dictionary.
  // Otherwise do the monomorphic cache probe.
  GenerateKeyedLoadReceiverCheck(
      masm, rdx, rax, Map::kHasNamedInterceptor, &lookup_monomorphic_cache);

  __ movq(rbx, FieldOperand(rdx, JSObject::kPropertiesOffset));
  __ CompareRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(not_equal, &lookup_monomorphic_cache);

  GenerateDictionaryLoad(masm, &slow_load, rbx, rcx, rax, rdi, rdi);
  __ IncrementCounter(counters->keyed_call_generic_lookup_dict(), 1);
  __ jmp(&do_call);

  __ bind(&lookup_monomorphic_cache);
  __ IncrementCounter(counters->keyed_call_generic_lookup_cache(), 1);
  GenerateMonomorphicCacheProbe(masm,
                                argc,
                                Code::KEYED_CALL_IC,
                                Code::kNoExtraICState);
  // Fall through on miss.

  __ bind(&slow_call);
  // This branch is taken if:
  // - the receiver requires boxing or access check,
  // - the key is neither smi nor a unique name,
  // - the value loaded is not a function,
  // - there is hope that the runtime will create a monomorphic call stub
  //   that will get fetched next time.
  __ IncrementCounter(counters->keyed_call_generic_slow(), 1);
  GenerateMiss(masm, argc);

  __ bind(&index_name);
  __ IndexFromHash(rbx, rcx);
  // Now jump to the place where smi keys are handled.
  __ jmp(&index_smi);
}


void KeyedCallIC::GenerateNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // rcx                 : function name
  // rsp[0]              : return address
  // rsp[8]              : argument argc
  // rsp[16]             : argument argc - 1
  // ...
  // rsp[argc * 8]       : argument 1
  // rsp[(argc + 1) * 8] : argument 0 = receiver
  // -----------------------------------

  // Check if the name is really a name.
  Label miss;
  __ JumpIfSmi(rcx, &miss);
  Condition cond = masm->IsObjectNameType(rcx, rax, rax);
  __ j(NegateCondition(cond), &miss);
  CallICBase::GenerateNormal(masm, argc);
  __ bind(&miss);
  GenerateMiss(masm, argc);
}


static Operand GenerateMappedArgumentsLookup(MacroAssembler* masm,
                                             Register object,
                                             Register key,
                                             Register scratch1,
                                             Register scratch2,
                                             Register scratch3,
                                             Label* unmapped_case,
                                             Label* slow_case) {
  Heap* heap = masm->isolate()->heap();

  // Check that the receiver is a JSObject. Because of the elements
  // map check later, we do not need to check for interceptors or
  // whether it requires access checks.
  __ JumpIfSmi(object, slow_case);
  // Check that the object is some kind of JSObject.
  __ CmpObjectType(object, FIRST_JS_RECEIVER_TYPE, scratch1);
  __ j(below, slow_case);

  // Check that the key is a positive smi.
  Condition check = masm->CheckNonNegativeSmi(key);
  __ j(NegateCondition(check), slow_case);

  // Load the elements into scratch1 and check its map. If not, jump
  // to the unmapped lookup with the parameter map in scratch1.
  Handle<Map> arguments_map(heap->non_strict_arguments_elements_map());
  __ movq(scratch1, FieldOperand(object, JSObject::kElementsOffset));
  __ CheckMap(scratch1, arguments_map, slow_case, DONT_DO_SMI_CHECK);

  // Check if element is in the range of mapped arguments.
  __ movq(scratch2, FieldOperand(scratch1, FixedArray::kLengthOffset));
  __ SmiSubConstant(scratch2, scratch2, Smi::FromInt(2));
  __ cmpq(key, scratch2);
  __ j(greater_equal, unmapped_case);

  // Load element index and check whether it is the hole.
  const int kHeaderSize = FixedArray::kHeaderSize + 2 * kPointerSize;
  __ SmiToInteger64(scratch3, key);
  __ movq(scratch2, FieldOperand(scratch1,
                                 scratch3,
                                 times_pointer_size,
                                 kHeaderSize));
  __ CompareRoot(scratch2, Heap::kTheHoleValueRootIndex);
  __ j(equal, unmapped_case);

  // Load value from context and return it. We can reuse scratch1 because
  // we do not jump to the unmapped lookup (which requires the parameter
  // map in scratch1).
  __ movq(scratch1, FieldOperand(scratch1, FixedArray::kHeaderSize));
  __ SmiToInteger64(scratch3, scratch2);
  return FieldOperand(scratch1,
                      scratch3,
                      times_pointer_size,
                      Context::kHeaderSize);
}


static Operand GenerateUnmappedArgumentsLookup(MacroAssembler* masm,
                                               Register key,
                                               Register parameter_map,
                                               Register scratch,
                                               Label* slow_case) {
  // Element is in arguments backing store, which is referenced by the
  // second element of the parameter_map. The parameter_map register
  // must be loaded with the parameter map of the arguments object and is
  // overwritten.
  const int kBackingStoreOffset = FixedArray::kHeaderSize + kPointerSize;
  Register backing_store = parameter_map;
  __ movq(backing_store, FieldOperand(parameter_map, kBackingStoreOffset));
  Handle<Map> fixed_array_map(masm->isolate()->heap()->fixed_array_map());
  __ CheckMap(backing_store, fixed_array_map, slow_case, DONT_DO_SMI_CHECK);
  __ movq(scratch, FieldOperand(backing_store, FixedArray::kLengthOffset));
  __ cmpq(key, scratch);
  __ j(greater_equal, slow_case);
  __ SmiToInteger64(scratch, key);
  return FieldOperand(backing_store,
                      scratch,
                      times_pointer_size,
                      FixedArray::kHeaderSize);
}


void KeyedLoadIC::GenerateNonStrictArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label slow, notin;
  Operand mapped_location =
      GenerateMappedArgumentsLookup(
          masm, rdx, rax, rbx, rcx, rdi, &notin, &slow);
  __ movq(rax, mapped_location);
  __ Ret();
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in rbx.
  Operand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, rax, rbx, rcx, &slow);
  __ CompareRoot(unmapped_location, Heap::kTheHoleValueRootIndex);
  __ j(equal, &slow);
  __ movq(rax, unmapped_location);
  __ Ret();
  __ bind(&slow);
  GenerateMiss(masm, MISS);
}


void KeyedStoreIC::GenerateNonStrictArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label slow, notin;
  Operand mapped_location = GenerateMappedArgumentsLookup(
      masm, rdx, rcx, rbx, rdi, r8, &notin, &slow);
  __ movq(mapped_location, rax);
  __ lea(r9, mapped_location);
  __ movq(r8, rax);
  __ RecordWrite(rbx,
                 r9,
                 r8,
                 kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET,
                 INLINE_SMI_CHECK);
  __ Ret();
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in rbx.
  Operand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, rcx, rbx, rdi, &slow);
  __ movq(unmapped_location, rax);
  __ lea(r9, unmapped_location);
  __ movq(r8, rax);
  __ RecordWrite(rbx,
                 r9,
                 r8,
                 kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET,
                 INLINE_SMI_CHECK);
  __ Ret();
  __ bind(&slow);
  GenerateMiss(masm, MISS);
}


void KeyedCallIC::GenerateNonStrictArguments(MacroAssembler* masm,
                                             int argc) {
  // ----------- S t a t e -------------
  // rcx                 : function name
  // rsp[0]              : return address
  // rsp[8]              : argument argc
  // rsp[16]             : argument argc - 1
  // ...
  // rsp[argc * 8]       : argument 1
  // rsp[(argc + 1) * 8] : argument 0 = receiver
  // -----------------------------------
  Label slow, notin;
  StackArgumentsAccessor args(rsp, argc);
  __ movq(rdx, args.GetReceiverOperand());
  Operand mapped_location = GenerateMappedArgumentsLookup(
      masm, rdx, rcx, rbx, rax, r8, &notin, &slow);
  __ movq(rdi, mapped_location);
  GenerateFunctionTailCall(masm, argc, &slow);
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in rbx.
  Operand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, rcx, rbx, rax, &slow);
  __ CompareRoot(unmapped_location, Heap::kTheHoleValueRootIndex);
  __ j(equal, &slow);
  __ movq(rdi, unmapped_location);
  GenerateFunctionTailCall(masm, argc, &slow);
  __ bind(&slow);
  GenerateMiss(masm, argc);
}


void LoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------

  // Probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(
      Code::HANDLER, MONOMORPHIC, Code::kNoExtraICState,
      Code::NORMAL, Code::LOAD_IC);
  masm->isolate()->stub_cache()->GenerateProbe(
      masm, flags, rax, rcx, rbx, rdx);

  GenerateMiss(masm);
}


void LoadIC::GenerateNormal(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  GenerateNameDictionaryReceiverCheck(masm, rax, rdx, rbx, &miss);

  //  rdx: elements
  // Search the dictionary placing the result in rax.
  GenerateDictionaryLoad(masm, &miss, rdx, rcx, rbx, rdi, rax);
  __ ret(0);

  // Cache miss: Jump to runtime.
  __ bind(&miss);
  GenerateMiss(masm);
}


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->load_miss(), 1);

  __ PopReturnAddressTo(rbx);
  __ push(rax);  // receiver
  __ push(rcx);  // name
  __ PushReturnAddressFrom(rbx);

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kLoadIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 2, 1);
}


void LoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------

  __ PopReturnAddressTo(rbx);
  __ push(rax);  // receiver
  __ push(rcx);  // name
  __ PushReturnAddressFrom(rbx);

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kGetProperty, 2, 1);
}


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm, ICMissMode miss_mode) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->keyed_load_miss(), 1);

  __ PopReturnAddressTo(rbx);
  __ push(rdx);  // receiver
  __ push(rax);  // name
  __ PushReturnAddressFrom(rbx);

  // Perform tail call to the entry.
  ExternalReference ref = miss_mode == MISS_FORCE_GENERIC
      ? ExternalReference(IC_Utility(kKeyedLoadIC_MissForceGeneric),
                          masm->isolate())
      : ExternalReference(IC_Utility(kKeyedLoadIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 2, 1);
}


void KeyedLoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------

  __ PopReturnAddressTo(rbx);
  __ push(rdx);  // receiver
  __ push(rax);  // name
  __ PushReturnAddressFrom(rbx);

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kKeyedGetProperty, 2, 1);
}


void StoreIC::GenerateMegamorphic(MacroAssembler* masm,
                                  StrictModeFlag strict_mode) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------

  // Get the receiver from the stack and probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(
      Code::HANDLER, MONOMORPHIC, strict_mode,
      Code::NORMAL, Code::STORE_IC);
  masm->isolate()->stub_cache()->GenerateProbe(
      masm, flags, rdx, rcx, rbx, no_reg);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
}


void StoreIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------

  __ PopReturnAddressTo(rbx);
  __ push(rdx);  // receiver
  __ push(rcx);  // name
  __ push(rax);  // value
  __ PushReturnAddressFrom(rbx);

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kStoreIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void StoreIC::GenerateNormal(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------

  Label miss;

  GenerateNameDictionaryReceiverCheck(masm, rdx, rbx, rdi, &miss);

  GenerateDictionaryStore(masm, &miss, rbx, rcx, rax, r8, r9);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->store_normal_hit(), 1);
  __ ret(0);

  __ bind(&miss);
  __ IncrementCounter(counters->store_normal_miss(), 1);
  GenerateMiss(masm);
}


void StoreIC::GenerateRuntimeSetProperty(MacroAssembler* masm,
                                         StrictModeFlag strict_mode) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  __ PopReturnAddressTo(rbx);
  __ push(rdx);
  __ push(rcx);
  __ push(rax);
  __ Push(Smi::FromInt(NONE));  // PropertyAttributes
  __ Push(Smi::FromInt(strict_mode));
  __ PushReturnAddressFrom(rbx);

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty, 5, 1);
}


void KeyedStoreIC::GenerateRuntimeSetProperty(MacroAssembler* masm,
                                              StrictModeFlag strict_mode) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------

  __ PopReturnAddressTo(rbx);
  __ push(rdx);  // receiver
  __ push(rcx);  // key
  __ push(rax);  // value
  __ Push(Smi::FromInt(NONE));          // PropertyAttributes
  __ Push(Smi::FromInt(strict_mode));   // Strict mode.
  __ PushReturnAddressFrom(rbx);

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty, 5, 1);
}


void StoreIC::GenerateSlow(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------

  __ PopReturnAddressTo(rbx);
  __ push(rdx);  // receiver
  __ push(rcx);  // key
  __ push(rax);  // value
  __ PushReturnAddressFrom(rbx);

  // Do tail-call to runtime routine.
  ExternalReference ref(IC_Utility(kStoreIC_Slow), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void KeyedStoreIC::GenerateSlow(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------

  __ PopReturnAddressTo(rbx);
  __ push(rdx);  // receiver
  __ push(rcx);  // key
  __ push(rax);  // value
  __ PushReturnAddressFrom(rbx);

  // Do tail-call to runtime routine.
  ExternalReference ref(IC_Utility(kKeyedStoreIC_Slow), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm, ICMissMode miss_mode) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------

  __ PopReturnAddressTo(rbx);
  __ push(rdx);  // receiver
  __ push(rcx);  // key
  __ push(rax);  // value
  __ PushReturnAddressFrom(rbx);

  // Do tail-call to runtime routine.
  ExternalReference ref = miss_mode == MISS_FORCE_GENERIC
    ? ExternalReference(IC_Utility(kKeyedStoreIC_MissForceGeneric),
                        masm->isolate())
    : ExternalReference(IC_Utility(kKeyedStoreIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


#undef __


Condition CompareIC::ComputeCondition(Token::Value op) {
  switch (op) {
    case Token::EQ_STRICT:
    case Token::EQ:
      return equal;
    case Token::LT:
      return less;
    case Token::GT:
      return greater;
    case Token::LTE:
      return less_equal;
    case Token::GTE:
      return greater_equal;
    default:
      UNREACHABLE();
      return no_condition;
  }
}


bool CompareIC::HasInlinedSmiCode(Address address) {
  // The address of the instruction following the call.
  Address test_instruction_address =
      address + Assembler::kCallTargetAddressOffset;

  // If the instruction following the call is not a test al, nothing
  // was inlined.
  return *test_instruction_address == Assembler::kTestAlByte;
}


void PatchInlinedSmiCode(Address address, InlinedSmiCheck check) {
  // The address of the instruction following the call.
  Address test_instruction_address =
      address + Assembler::kCallTargetAddressOffset;

  // If the instruction following the call is not a test al, nothing
  // was inlined.
  if (*test_instruction_address != Assembler::kTestAlByte) {
    ASSERT(*test_instruction_address == Assembler::kNopByte);
    return;
  }

  Address delta_address = test_instruction_address + 1;
  // The delta to the start of the map check instruction and the
  // condition code uses at the patched jump.
  int8_t delta = *reinterpret_cast<int8_t*>(delta_address);
  if (FLAG_trace_ic) {
    PrintF("[  patching ic at %p, test=%p, delta=%d\n",
           address, test_instruction_address, delta);
  }

  // Patch with a short conditional jump. Enabling means switching from a short
  // jump-if-carry/not-carry to jump-if-zero/not-zero, whereas disabling is the
  // reverse operation of that.
  Address jmp_address = test_instruction_address - delta;
  ASSERT((check == ENABLE_INLINED_SMI_CHECK)
         ? (*jmp_address == Assembler::kJncShortOpcode ||
            *jmp_address == Assembler::kJcShortOpcode)
         : (*jmp_address == Assembler::kJnzShortOpcode ||
            *jmp_address == Assembler::kJzShortOpcode));
  Condition cc = (check == ENABLE_INLINED_SMI_CHECK)
      ? (*jmp_address == Assembler::kJncShortOpcode ? not_zero : zero)
      : (*jmp_address == Assembler::kJnzShortOpcode ? not_carry : carry);
  *jmp_address = static_cast<byte>(Assembler::kJccShortPrefix | cc);
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
