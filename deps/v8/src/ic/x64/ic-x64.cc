// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_X64

#include "src/codegen.h"
#include "src/ic/ic.h"
#include "src/ic/ic-compiler.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Static IC stub generators.
//

#define __ ACCESS_MASM(masm)


static void GenerateGlobalInstanceTypeCheck(MacroAssembler* masm, Register type,
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


// Helper function used to load a property from a dictionary backing storage.
// This function may return false negatives, so miss_label
// must always call a backup property load that is complete.
// This function is safe to call if name is not an internalized string,
// and will jump to the miss_label in that case.
// The generated code assumes that the receiver has slow properties,
// is not a global object and does not have interceptors.
static void GenerateDictionaryLoad(MacroAssembler* masm, Label* miss_label,
                                   Register elements, Register name,
                                   Register r0, Register r1, Register result) {
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
  NameDictionaryLookupStub::GeneratePositiveLookup(masm, miss_label, &done,
                                                   elements, name, r0, r1);

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
  __ movp(result, Operand(elements, r1, times_pointer_size,
                          kValueOffset - kHeapObjectTag));
}


// Helper function used to store a property to a dictionary backing
// storage. This function may fail to store a property even though it
// is in the dictionary, so code at miss_label must always call a
// backup property store that is complete. This function is safe to
// call if name is not an internalized string, and will jump to the miss_label
// in that case. The generated code assumes that the receiver has slow
// properties, is not a global object and does not have interceptors.
static void GenerateDictionaryStore(MacroAssembler* masm, Label* miss_label,
                                    Register elements, Register name,
                                    Register value, Register scratch0,
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
  NameDictionaryLookupStub::GeneratePositiveLookup(
      masm, miss_label, &done, elements, name, scratch0, scratch1);

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
       PropertyDetails::AttributesField::encode(READ_ONLY))
      << kSmiTagSize;
  __ Test(Operand(elements, scratch1, times_pointer_size,
                  kDetailsOffset - kHeapObjectTag),
          Smi::FromInt(kTypeAndReadOnlyMask));
  __ j(not_zero, miss_label);

  // Store the value at the masked, scaled index.
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ leap(scratch1, Operand(elements, scratch1, times_pointer_size,
                            kValueOffset - kHeapObjectTag));
  __ movp(Operand(scratch1, 0), value);

  // Update write barrier. Make sure not to clobber the value.
  __ movp(scratch0, value);
  __ RecordWrite(elements, scratch1, scratch0, kDontSaveFPRegs);
}


// Checks the receiver for special cases (value type, slow case bits).
// Falls through for regular JS object.
static void GenerateKeyedLoadReceiverCheck(MacroAssembler* masm,
                                           Register receiver, Register map,
                                           int interceptor_bit, Label* slow) {
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
  DCHECK(JS_OBJECT_TYPE > JS_VALUE_TYPE);
  __ CmpObjectType(receiver, JS_OBJECT_TYPE, map);
  __ j(below, slow);

  // Check bit field.
  __ testb(
      FieldOperand(map, Map::kBitFieldOffset),
      Immediate((1 << Map::kIsAccessCheckNeeded) | (1 << interceptor_bit)));
  __ j(not_zero, slow);
}


// Loads an indexed element from a fast case array.
// If not_fast_array is NULL, doesn't perform the elements map check.
static void GenerateFastArrayLoad(MacroAssembler* masm, Register receiver,
                                  Register key, Register elements,
                                  Register scratch, Register result,
                                  Label* not_fast_array, Label* out_of_range) {
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

  __ movp(elements, FieldOperand(receiver, JSObject::kElementsOffset));
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
  __ movp(scratch, FieldOperand(elements, index.reg, index.scale,
                                FixedArray::kHeaderSize));
  __ CompareRoot(scratch, Heap::kTheHoleValueRootIndex);
  // In case the loaded value is the_hole we have to consult GetProperty
  // to ensure the prototype chain is searched.
  __ j(equal, out_of_range);
  if (!result.is(scratch)) {
    __ movp(result, scratch);
  }
}


// Checks whether a key is an array index string or a unique name.
// Falls through if the key is a unique name.
static void GenerateKeyNameCheck(MacroAssembler* masm, Register key,
                                 Register map, Register hash,
                                 Label* index_string, Label* not_unique) {
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
  // The return address is on the stack.
  Label slow, check_name, index_smi, index_name, property_array_property;
  Label probe_dictionary, check_number_dictionary;

  Register receiver = LoadDescriptor::ReceiverRegister();
  Register key = LoadDescriptor::NameRegister();
  DCHECK(receiver.is(rdx));
  DCHECK(key.is(rcx));

  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &check_name);
  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from below
  // where a numeric string is converted to a smi.

  GenerateKeyedLoadReceiverCheck(masm, receiver, rax,
                                 Map::kHasIndexedInterceptor, &slow);

  // Check the receiver's map to see if it has fast elements.
  __ CheckFastElements(rax, &check_number_dictionary);

  GenerateFastArrayLoad(masm, receiver, key, rax, rbx, rax, NULL, &slow);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->keyed_load_generic_smi(), 1);
  __ ret(0);

  __ bind(&check_number_dictionary);
  __ SmiToInteger32(rbx, key);
  __ movp(rax, FieldOperand(receiver, JSObject::kElementsOffset));

  // Check whether the elements is a number dictionary.
  // rbx: key as untagged int32
  // rax: elements
  __ CompareRoot(FieldOperand(rax, HeapObject::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(not_equal, &slow);
  __ LoadFromNumberDictionary(&slow, rax, key, rbx, r9, rdi, rax);
  __ ret(0);

  __ bind(&slow);
  // Slow case: Jump to runtime.
  __ IncrementCounter(counters->keyed_load_generic_slow(), 1);
  GenerateRuntimeGetProperty(masm);

  __ bind(&check_name);
  GenerateKeyNameCheck(masm, key, rax, rbx, &index_name, &slow);

  GenerateKeyedLoadReceiverCheck(masm, receiver, rax, Map::kHasNamedInterceptor,
                                 &slow);

  // If the receiver is a fast-case object, check the keyed lookup
  // cache. Otherwise probe the dictionary leaving result in key.
  __ movp(rbx, FieldOperand(receiver, JSObject::kPropertiesOffset));
  __ CompareRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(equal, &probe_dictionary);

  // Load the map of the receiver, compute the keyed lookup cache hash
  // based on 32 bits of the map pointer and the string hash.
  __ movp(rbx, FieldOperand(receiver, HeapObject::kMapOffset));
  __ movl(rax, rbx);
  __ shrl(rax, Immediate(KeyedLookupCache::kMapHashShift));
  __ movl(rdi, FieldOperand(key, String::kHashFieldOffset));
  __ shrl(rdi, Immediate(String::kHashShift));
  __ xorp(rax, rdi);
  int mask = (KeyedLookupCache::kCapacityMask & KeyedLookupCache::kHashMask);
  __ andp(rax, Immediate(mask));

  // Load the key (consisting of map and internalized string) from the cache and
  // check for match.
  Label load_in_object_property;
  static const int kEntriesPerBucket = KeyedLookupCache::kEntriesPerBucket;
  Label hit_on_nth_entry[kEntriesPerBucket];
  ExternalReference cache_keys =
      ExternalReference::keyed_lookup_cache_keys(masm->isolate());

  for (int i = 0; i < kEntriesPerBucket - 1; i++) {
    Label try_next_entry;
    __ movp(rdi, rax);
    __ shlp(rdi, Immediate(kPointerSizeLog2 + 1));
    __ LoadAddress(kScratchRegister, cache_keys);
    int off = kPointerSize * i * 2;
    __ cmpp(rbx, Operand(kScratchRegister, rdi, times_1, off));
    __ j(not_equal, &try_next_entry);
    __ cmpp(key, Operand(kScratchRegister, rdi, times_1, off + kPointerSize));
    __ j(equal, &hit_on_nth_entry[i]);
    __ bind(&try_next_entry);
  }

  int off = kPointerSize * (kEntriesPerBucket - 1) * 2;
  __ cmpp(rbx, Operand(kScratchRegister, rdi, times_1, off));
  __ j(not_equal, &slow);
  __ cmpp(key, Operand(kScratchRegister, rdi, times_1, off + kPointerSize));
  __ j(not_equal, &slow);

  // Get field offset, which is a 32-bit integer.
  ExternalReference cache_field_offsets =
      ExternalReference::keyed_lookup_cache_field_offsets(masm->isolate());

  // Hit on nth entry.
  for (int i = kEntriesPerBucket - 1; i >= 0; i--) {
    __ bind(&hit_on_nth_entry[i]);
    if (i != 0) {
      __ addl(rax, Immediate(i));
    }
    __ LoadAddress(kScratchRegister, cache_field_offsets);
    __ movl(rdi, Operand(kScratchRegister, rax, times_4, 0));
    __ movzxbp(rax, FieldOperand(rbx, Map::kInObjectPropertiesOffset));
    __ subp(rdi, rax);
    __ j(above_equal, &property_array_property);
    if (i != 0) {
      __ jmp(&load_in_object_property);
    }
  }

  // Load in-object property.
  __ bind(&load_in_object_property);
  __ movzxbp(rax, FieldOperand(rbx, Map::kInstanceSizeOffset));
  __ addp(rax, rdi);
  __ movp(rax, FieldOperand(receiver, rax, times_pointer_size, 0));
  __ IncrementCounter(counters->keyed_load_generic_lookup_cache(), 1);
  __ ret(0);

  // Load property array property.
  __ bind(&property_array_property);
  __ movp(rax, FieldOperand(receiver, JSObject::kPropertiesOffset));
  __ movp(rax,
          FieldOperand(rax, rdi, times_pointer_size, FixedArray::kHeaderSize));
  __ IncrementCounter(counters->keyed_load_generic_lookup_cache(), 1);
  __ ret(0);

  // Do a quick inline probe of the receiver's dictionary, if it
  // exists.
  __ bind(&probe_dictionary);
  // rbx: elements

  __ movp(rax, FieldOperand(receiver, JSObject::kMapOffset));
  __ movb(rax, FieldOperand(rax, Map::kInstanceTypeOffset));
  GenerateGlobalInstanceTypeCheck(masm, rax, &slow);

  GenerateDictionaryLoad(masm, &slow, rbx, key, rax, rdi, rax);
  __ IncrementCounter(counters->keyed_load_generic_symbol(), 1);
  __ ret(0);

  __ bind(&index_name);
  __ IndexFromHash(rbx, key);
  __ jmp(&index_smi);
}


static void KeyedStoreGenerateMegamorphicHelper(
    MacroAssembler* masm, Label* fast_object, Label* fast_double, Label* slow,
    KeyedStoreCheckMap check_map, KeyedStoreIncrementLength increment_length) {
  Label transition_smi_elements;
  Label finish_object_store, non_double_value, transition_double_elements;
  Label fast_double_without_map_check;
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register key = StoreDescriptor::NameRegister();
  Register value = StoreDescriptor::ValueRegister();
  DCHECK(receiver.is(rdx));
  DCHECK(key.is(rcx));
  DCHECK(value.is(rax));
  // Fast case: Do the store, could be either Object or double.
  __ bind(fast_object);
  // rbx: receiver's elements array (a FixedArray)
  // receiver is a JSArray.
  // r9: map of receiver
  if (check_map == kCheckMap) {
    __ movp(rdi, FieldOperand(rbx, HeapObject::kMapOffset));
    __ CompareRoot(rdi, Heap::kFixedArrayMapRootIndex);
    __ j(not_equal, fast_double);
  }

  // HOLECHECK: guards "A[i] = V"
  // We have to go to the runtime if the current value is the hole because
  // there may be a callback on the element
  Label holecheck_passed1;
  __ movp(kScratchRegister,
          FieldOperand(rbx, key, times_pointer_size, FixedArray::kHeaderSize));
  __ CompareRoot(kScratchRegister, Heap::kTheHoleValueRootIndex);
  __ j(not_equal, &holecheck_passed1);
  __ JumpIfDictionaryInPrototypeChain(receiver, rdi, kScratchRegister, slow);

  __ bind(&holecheck_passed1);

  // Smi stores don't require further checks.
  Label non_smi_value;
  __ JumpIfNotSmi(value, &non_smi_value);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ leal(rdi, Operand(key, 1));
    __ Integer32ToSmiField(FieldOperand(receiver, JSArray::kLengthOffset), rdi);
  }
  // It's irrelevant whether array is smi-only or not when writing a smi.
  __ movp(FieldOperand(rbx, key, times_pointer_size, FixedArray::kHeaderSize),
          value);
  __ ret(0);

  __ bind(&non_smi_value);
  // Writing a non-smi, check whether array allows non-smi elements.
  // r9: receiver's map
  __ CheckFastObjectElements(r9, &transition_smi_elements);

  __ bind(&finish_object_store);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ leal(rdi, Operand(key, 1));
    __ Integer32ToSmiField(FieldOperand(receiver, JSArray::kLengthOffset), rdi);
  }
  __ movp(FieldOperand(rbx, key, times_pointer_size, FixedArray::kHeaderSize),
          value);
  __ movp(rdx, value);  // Preserve the value which is returned.
  __ RecordWriteArray(rbx, rdx, key, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ ret(0);

  __ bind(fast_double);
  if (check_map == kCheckMap) {
    // Check for fast double array case. If this fails, call through to the
    // runtime.
    // rdi: elements array's map
    __ CompareRoot(rdi, Heap::kFixedDoubleArrayMapRootIndex);
    __ j(not_equal, slow);
  }

  // HOLECHECK: guards "A[i] double hole?"
  // We have to see if the double version of the hole is present. If so
  // go to the runtime.
  uint32_t offset = FixedDoubleArray::kHeaderSize + sizeof(kHoleNanLower32);
  __ cmpl(FieldOperand(rbx, key, times_8, offset), Immediate(kHoleNanUpper32));
  __ j(not_equal, &fast_double_without_map_check);
  __ JumpIfDictionaryInPrototypeChain(receiver, rdi, kScratchRegister, slow);

  __ bind(&fast_double_without_map_check);
  __ StoreNumberToDoubleElements(value, rbx, key, xmm0,
                                 &transition_double_elements);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ leal(rdi, Operand(key, 1));
    __ Integer32ToSmiField(FieldOperand(receiver, JSArray::kLengthOffset), rdi);
  }
  __ ret(0);

  __ bind(&transition_smi_elements);
  __ movp(rbx, FieldOperand(receiver, HeapObject::kMapOffset));

  // Transition the array appropriately depending on the value type.
  __ movp(r9, FieldOperand(value, HeapObject::kMapOffset));
  __ CompareRoot(r9, Heap::kHeapNumberMapRootIndex);
  __ j(not_equal, &non_double_value);

  // Value is a double. Transition FAST_SMI_ELEMENTS ->
  // FAST_DOUBLE_ELEMENTS and complete the store.
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                         FAST_DOUBLE_ELEMENTS, rbx, rdi, slow);
  AllocationSiteMode mode =
      AllocationSite::GetMode(FAST_SMI_ELEMENTS, FAST_DOUBLE_ELEMENTS);
  ElementsTransitionGenerator::GenerateSmiToDouble(masm, receiver, key, value,
                                                   rbx, mode, slow);
  __ movp(rbx, FieldOperand(receiver, JSObject::kElementsOffset));
  __ jmp(&fast_double_without_map_check);

  __ bind(&non_double_value);
  // Value is not a double, FAST_SMI_ELEMENTS -> FAST_ELEMENTS
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS, FAST_ELEMENTS, rbx,
                                         rdi, slow);
  mode = AllocationSite::GetMode(FAST_SMI_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
      masm, receiver, key, value, rbx, mode, slow);
  __ movp(rbx, FieldOperand(receiver, JSObject::kElementsOffset));
  __ jmp(&finish_object_store);

  __ bind(&transition_double_elements);
  // Elements are FAST_DOUBLE_ELEMENTS, but value is an Object that's not a
  // HeapNumber. Make sure that the receiver is a Array with FAST_ELEMENTS and
  // transition array from FAST_DOUBLE_ELEMENTS to FAST_ELEMENTS
  __ movp(rbx, FieldOperand(receiver, HeapObject::kMapOffset));
  __ LoadTransitionedArrayMapConditional(FAST_DOUBLE_ELEMENTS, FAST_ELEMENTS,
                                         rbx, rdi, slow);
  mode = AllocationSite::GetMode(FAST_DOUBLE_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateDoubleToObject(masm, receiver, key,
                                                      value, rbx, mode, slow);
  __ movp(rbx, FieldOperand(receiver, JSObject::kElementsOffset));
  __ jmp(&finish_object_store);
}


void KeyedStoreIC::GenerateMegamorphic(MacroAssembler* masm,
                                       StrictMode strict_mode) {
  // Return address is on the stack.
  Label slow, slow_with_tagged_index, fast_object, fast_object_grow;
  Label fast_double, fast_double_grow;
  Label array, extra, check_if_double_array, maybe_name_key, miss;
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register key = StoreDescriptor::NameRegister();
  DCHECK(receiver.is(rdx));
  DCHECK(key.is(rcx));

  // Check that the object isn't a smi.
  __ JumpIfSmi(receiver, &slow_with_tagged_index);
  // Get the map from the receiver.
  __ movp(r9, FieldOperand(receiver, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks and is not observed.
  // The generic stub does not perform map checks or handle observed objects.
  __ testb(FieldOperand(r9, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsAccessCheckNeeded | 1 << Map::kIsObserved));
  __ j(not_zero, &slow_with_tagged_index);
  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &maybe_name_key);
  __ SmiToInteger32(key, key);

  __ CmpInstanceType(r9, JS_ARRAY_TYPE);
  __ j(equal, &array);
  // Check that the object is some kind of JSObject.
  __ CmpInstanceType(r9, FIRST_JS_OBJECT_TYPE);
  __ j(below, &slow);

  // Object case: Check key against length in the elements array.
  __ movp(rbx, FieldOperand(receiver, JSObject::kElementsOffset));
  // Check array bounds.
  __ SmiCompareInteger32(FieldOperand(rbx, FixedArray::kLengthOffset), key);
  // rbx: FixedArray
  __ j(above, &fast_object);

  // Slow case: call runtime.
  __ bind(&slow);
  __ Integer32ToSmi(key, key);
  __ bind(&slow_with_tagged_index);
  PropertyICCompiler::GenerateRuntimeSetProperty(masm, strict_mode);
  // Never returns to here.

  __ bind(&maybe_name_key);
  __ movp(r9, FieldOperand(key, HeapObject::kMapOffset));
  __ movzxbp(r9, FieldOperand(r9, Map::kInstanceTypeOffset));
  __ JumpIfNotUniqueNameInstanceType(r9, &slow_with_tagged_index);
  Code::Flags flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::STORE_IC));
  masm->isolate()->stub_cache()->GenerateProbe(masm, flags, false, receiver,
                                               key, rbx, no_reg);
  // Cache miss.
  __ jmp(&miss);

  // Extra capacity case: Check if there is extra capacity to
  // perform the store and update the length. Used for adding one
  // element to the array by writing to array[array.length].
  __ bind(&extra);
  // receiver is a JSArray.
  // rbx: receiver's elements array (a FixedArray)
  // flags: smicompare (receiver.length(), rbx)
  __ j(not_equal, &slow);  // do not leave holes in the array
  __ SmiCompareInteger32(FieldOperand(rbx, FixedArray::kLengthOffset), key);
  __ j(below_equal, &slow);
  // Increment index to get new length.
  __ movp(rdi, FieldOperand(rbx, HeapObject::kMapOffset));
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
  // receiver is a JSArray.
  __ movp(rbx, FieldOperand(receiver, JSObject::kElementsOffset));

  // Check the key against the length in the array, compute the
  // address to store into and fall through to fast case.
  __ SmiCompareInteger32(FieldOperand(receiver, JSArray::kLengthOffset), key);
  __ j(below_equal, &extra);

  KeyedStoreGenerateMegamorphicHelper(masm, &fast_object, &fast_double, &slow,
                                      kCheckMap, kDontIncrementLength);
  KeyedStoreGenerateMegamorphicHelper(masm, &fast_object_grow,
                                      &fast_double_grow, &slow, kDontCheckMap,
                                      kIncrementLength);

  __ bind(&miss);
  GenerateMiss(masm);
}


static Operand GenerateMappedArgumentsLookup(
    MacroAssembler* masm, Register object, Register key, Register scratch1,
    Register scratch2, Register scratch3, Label* unmapped_case,
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
  Handle<Map> arguments_map(heap->sloppy_arguments_elements_map());
  __ movp(scratch1, FieldOperand(object, JSObject::kElementsOffset));
  __ CheckMap(scratch1, arguments_map, slow_case, DONT_DO_SMI_CHECK);

  // Check if element is in the range of mapped arguments.
  __ movp(scratch2, FieldOperand(scratch1, FixedArray::kLengthOffset));
  __ SmiSubConstant(scratch2, scratch2, Smi::FromInt(2));
  __ cmpp(key, scratch2);
  __ j(greater_equal, unmapped_case);

  // Load element index and check whether it is the hole.
  const int kHeaderSize = FixedArray::kHeaderSize + 2 * kPointerSize;
  __ SmiToInteger64(scratch3, key);
  __ movp(scratch2,
          FieldOperand(scratch1, scratch3, times_pointer_size, kHeaderSize));
  __ CompareRoot(scratch2, Heap::kTheHoleValueRootIndex);
  __ j(equal, unmapped_case);

  // Load value from context and return it. We can reuse scratch1 because
  // we do not jump to the unmapped lookup (which requires the parameter
  // map in scratch1).
  __ movp(scratch1, FieldOperand(scratch1, FixedArray::kHeaderSize));
  __ SmiToInteger64(scratch3, scratch2);
  return FieldOperand(scratch1, scratch3, times_pointer_size,
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
  __ movp(backing_store, FieldOperand(parameter_map, kBackingStoreOffset));
  Handle<Map> fixed_array_map(masm->isolate()->heap()->fixed_array_map());
  __ CheckMap(backing_store, fixed_array_map, slow_case, DONT_DO_SMI_CHECK);
  __ movp(scratch, FieldOperand(backing_store, FixedArray::kLengthOffset));
  __ cmpp(key, scratch);
  __ j(greater_equal, slow_case);
  __ SmiToInteger64(scratch, key);
  return FieldOperand(backing_store, scratch, times_pointer_size,
                      FixedArray::kHeaderSize);
}


void KeyedStoreIC::GenerateSloppyArguments(MacroAssembler* masm) {
  // The return address is on the stack.
  Label slow, notin;
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  Register value = StoreDescriptor::ValueRegister();
  DCHECK(receiver.is(rdx));
  DCHECK(name.is(rcx));
  DCHECK(value.is(rax));

  Operand mapped_location = GenerateMappedArgumentsLookup(
      masm, receiver, name, rbx, rdi, r8, &notin, &slow);
  __ movp(mapped_location, value);
  __ leap(r9, mapped_location);
  __ movp(r8, value);
  __ RecordWrite(rbx, r9, r8, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                 INLINE_SMI_CHECK);
  __ Ret();
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in rbx.
  Operand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, name, rbx, rdi, &slow);
  __ movp(unmapped_location, value);
  __ leap(r9, unmapped_location);
  __ movp(r8, value);
  __ RecordWrite(rbx, r9, r8, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                 INLINE_SMI_CHECK);
  __ Ret();
  __ bind(&slow);
  GenerateMiss(masm);
}


void LoadIC::GenerateNormal(MacroAssembler* masm) {
  Register dictionary = rax;
  DCHECK(!dictionary.is(LoadDescriptor::ReceiverRegister()));
  DCHECK(!dictionary.is(LoadDescriptor::NameRegister()));

  Label slow;

  __ movp(dictionary, FieldOperand(LoadDescriptor::ReceiverRegister(),
                                   JSObject::kPropertiesOffset));
  GenerateDictionaryLoad(masm, &slow, dictionary,
                         LoadDescriptor::NameRegister(), rbx, rdi, rax);
  __ ret(0);

  // Dictionary load failed, go slow (but don't miss).
  __ bind(&slow);
  GenerateRuntimeGetProperty(masm);
}


// A register that isn't one of the parameters to the load ic.
static const Register LoadIC_TempRegister() { return rbx; }


static const Register KeyedLoadIC_TempRegister() { return rbx; }


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // The return address is on the stack.

  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->load_miss(), 1);

  __ PopReturnAddressTo(LoadIC_TempRegister());
  __ Push(LoadDescriptor::ReceiverRegister());  // receiver
  __ Push(LoadDescriptor::NameRegister());      // name
  __ PushReturnAddressFrom(LoadIC_TempRegister());

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kLoadIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 2, 1);
}


void LoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // The return address is on the stack.

  __ PopReturnAddressTo(LoadIC_TempRegister());
  __ Push(LoadDescriptor::ReceiverRegister());  // receiver
  __ Push(LoadDescriptor::NameRegister());      // name
  __ PushReturnAddressFrom(LoadIC_TempRegister());

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kGetProperty, 2, 1);
}


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  // The return address is on the stack.
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->keyed_load_miss(), 1);

  __ PopReturnAddressTo(KeyedLoadIC_TempRegister());
  __ Push(LoadDescriptor::ReceiverRegister());  // receiver
  __ Push(LoadDescriptor::NameRegister());      // name
  __ PushReturnAddressFrom(KeyedLoadIC_TempRegister());

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kKeyedLoadIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 2, 1);
}


void KeyedLoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // The return address is on the stack.

  __ PopReturnAddressTo(KeyedLoadIC_TempRegister());
  __ Push(LoadDescriptor::ReceiverRegister());  // receiver
  __ Push(LoadDescriptor::NameRegister());      // name
  __ PushReturnAddressFrom(KeyedLoadIC_TempRegister());

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kKeyedGetProperty, 2, 1);
}


void StoreIC::GenerateMegamorphic(MacroAssembler* masm) {
  // The return address is on the stack.

  // Get the receiver from the stack and probe the stub cache.
  Code::Flags flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::STORE_IC));
  masm->isolate()->stub_cache()->GenerateProbe(
      masm, flags, false, StoreDescriptor::ReceiverRegister(),
      StoreDescriptor::NameRegister(), rbx, no_reg);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
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


void StoreIC::GenerateMiss(MacroAssembler* masm) {
  // Return address is on the stack.
  StoreIC_PushArgs(masm);

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kStoreIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void StoreIC::GenerateNormal(MacroAssembler* masm) {
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  Register value = StoreDescriptor::ValueRegister();
  Register dictionary = rbx;

  Label miss;

  __ movp(dictionary, FieldOperand(receiver, JSObject::kPropertiesOffset));
  GenerateDictionaryStore(masm, &miss, dictionary, name, value, r8, r9);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->store_normal_hit(), 1);
  __ ret(0);

  __ bind(&miss);
  __ IncrementCounter(counters->store_normal_miss(), 1);
  GenerateMiss(masm);
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm) {
  // Return address is on the stack.
  StoreIC_PushArgs(masm);

  // Do tail-call to runtime routine.
  ExternalReference ref =
      ExternalReference(IC_Utility(kKeyedStoreIC_Miss), masm->isolate());
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
    DCHECK(*test_instruction_address == Assembler::kNopByte);
    return;
  }

  Address delta_address = test_instruction_address + 1;
  // The delta to the start of the map check instruction and the
  // condition code uses at the patched jump.
  uint8_t delta = *reinterpret_cast<uint8_t*>(delta_address);
  if (FLAG_trace_ic) {
    PrintF("[  patching ic at %p, test=%p, delta=%d\n", address,
           test_instruction_address, delta);
  }

  // Patch with a short conditional jump. Enabling means switching from a short
  // jump-if-carry/not-carry to jump-if-zero/not-zero, whereas disabling is the
  // reverse operation of that.
  Address jmp_address = test_instruction_address - delta;
  DCHECK((check == ENABLE_INLINED_SMI_CHECK)
             ? (*jmp_address == Assembler::kJncShortOpcode ||
                *jmp_address == Assembler::kJcShortOpcode)
             : (*jmp_address == Assembler::kJnzShortOpcode ||
                *jmp_address == Assembler::kJzShortOpcode));
  Condition cc =
      (check == ENABLE_INLINED_SMI_CHECK)
          ? (*jmp_address == Assembler::kJncShortOpcode ? not_zero : zero)
          : (*jmp_address == Assembler::kJnzShortOpcode ? not_carry : carry);
  *jmp_address = static_cast<byte>(Assembler::kJccShortPrefix | cc);
}
}
}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
