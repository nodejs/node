// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_IA32

#include "src/codegen.h"
#include "src/ic-inl.h"
#include "src/runtime.h"
#include "src/stub-cache.h"

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
  __ cmp(type, JS_GLOBAL_OBJECT_TYPE);
  __ j(equal, global_object);
  __ cmp(type, JS_BUILTINS_OBJECT_TYPE);
  __ j(equal, global_object);
  __ cmp(type, JS_GLOBAL_PROXY_TYPE);
  __ j(equal, global_object);
}


// Helper function used to load a property from a dictionary backing
// storage. This function may fail to load a property even though it is
// in the dictionary, so code at miss_label must always call a backup
// property load that is complete. This function is safe to call if
// name is not internalized, and will jump to the miss_label in that
// case. The generated code assumes that the receiver has slow
// properties, is not a global object and does not have interceptors.
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
  // Scratch registers:
  //
  // r0   - used for the index into the property dictionary
  //
  // r1   - used to hold the capacity of the property dictionary.
  //
  // result - holds the result on exit.

  Label done;

  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm,
                                                   miss_label,
                                                   &done,
                                                   elements,
                                                   name,
                                                   r0,
                                                   r1);

  // If probing finds an entry in the dictionary, r0 contains the
  // index into the dictionary. Check that the value is a normal
  // property.
  __ bind(&done);
  const int kElementsStartOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;
  const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  __ test(Operand(elements, r0, times_4, kDetailsOffset - kHeapObjectTag),
          Immediate(PropertyDetails::TypeField::kMask << kSmiTagSize));
  __ j(not_zero, miss_label);

  // Get the value at the masked, scaled index.
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ mov(result, Operand(elements, r0, times_4, kValueOffset - kHeapObjectTag));
}


// Helper function used to store a property to a dictionary backing
// storage. This function may fail to store a property eventhough it
// is in the dictionary, so code at miss_label must always call a
// backup property store that is complete. This function is safe to
// call if name is not internalized, and will jump to the miss_label in
// that case. The generated code assumes that the receiver has slow
// properties, is not a global object and does not have interceptors.
static void GenerateDictionaryStore(MacroAssembler* masm,
                                    Label* miss_label,
                                    Register elements,
                                    Register name,
                                    Register value,
                                    Register r0,
                                    Register r1) {
  // Register use:
  //
  // elements - holds the property dictionary on entry and is clobbered.
  //
  // name - holds the name of the property on entry and is unchanged.
  //
  // value - holds the value to store and is unchanged.
  //
  // r0 - used for index into the property dictionary and is clobbered.
  //
  // r1 - used to hold the capacity of the property dictionary and is clobbered.
  Label done;


  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm,
                                                   miss_label,
                                                   &done,
                                                   elements,
                                                   name,
                                                   r0,
                                                   r1);

  // If probing finds an entry in the dictionary, r0 contains the
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
  __ test(Operand(elements, r0, times_4, kDetailsOffset - kHeapObjectTag),
          Immediate(kTypeAndReadOnlyMask));
  __ j(not_zero, miss_label);

  // Store the value at the masked, scaled index.
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ lea(r0, Operand(elements, r0, times_4, kValueOffset - kHeapObjectTag));
  __ mov(Operand(r0, 0), value);

  // Update write barrier. Make sure not to clobber the value.
  __ mov(r1, value);
  __ RecordWrite(elements, r0, r1, kDontSaveFPRegs);
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

  // Get the map of the receiver.
  __ mov(map, FieldOperand(receiver, HeapObject::kMapOffset));

  // Check bit field.
  __ test_b(FieldOperand(map, Map::kBitFieldOffset),
            (1 << Map::kIsAccessCheckNeeded) | (1 << interceptor_bit));
  __ j(not_zero, slow);
  // Check that the object is some kind of JS object EXCEPT JS Value type.
  // In the case that the object is a value-wrapper object,
  // we enter the runtime system to make sure that indexing
  // into string objects works as intended.
  DCHECK(JS_OBJECT_TYPE > JS_VALUE_TYPE);

  __ CmpInstanceType(map, JS_OBJECT_TYPE);
  __ j(below, slow);
}


// Loads an indexed element from a fast case array.
// If not_fast_array is NULL, doesn't perform the elements map check.
static void GenerateFastArrayLoad(MacroAssembler* masm,
                                  Register receiver,
                                  Register key,
                                  Register scratch,
                                  Register result,
                                  Label* not_fast_array,
                                  Label* out_of_range) {
  // Register use:
  //   receiver - holds the receiver and is unchanged.
  //   key - holds the key and is unchanged (must be a smi).
  // Scratch registers:
  //   scratch - used to hold elements of the receiver and the loaded value.
  //   result - holds the result on exit if the load succeeds and
  //            we fall through.

  __ mov(scratch, FieldOperand(receiver, JSObject::kElementsOffset));
  if (not_fast_array != NULL) {
    // Check that the object is in fast mode and writable.
    __ CheckMap(scratch,
                masm->isolate()->factory()->fixed_array_map(),
                not_fast_array,
                DONT_DO_SMI_CHECK);
  } else {
    __ AssertFastElements(scratch);
  }
  // Check that the key (index) is within bounds.
  __ cmp(key, FieldOperand(scratch, FixedArray::kLengthOffset));
  __ j(above_equal, out_of_range);
  // Fast case: Do the load.
  STATIC_ASSERT((kPointerSize == 4) && (kSmiTagSize == 1) && (kSmiTag == 0));
  __ mov(scratch, FieldOperand(scratch, key, times_2, FixedArray::kHeaderSize));
  __ cmp(scratch, Immediate(masm->isolate()->factory()->the_hole_value()));
  // In case the loaded value is the_hole we have to consult GetProperty
  // to ensure the prototype chain is searched.
  __ j(equal, out_of_range);
  if (!result.is(scratch)) {
    __ mov(result, scratch);
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
  __ mov(hash, FieldOperand(key, Name::kHashFieldOffset));
  __ test(hash, Immediate(Name::kContainsCachedArrayIndexMask));
  __ j(zero, index_string);

  // Is the string internalized? We already know it's a string so a single
  // bit test is enough.
  STATIC_ASSERT(kNotInternalizedTag != 0);
  __ test_b(FieldOperand(map, Map::kInstanceTypeOffset),
            kIsNotInternalizedMask);
  __ j(not_zero, not_unique);

  __ bind(&unique);
}


static Operand GenerateMappedArgumentsLookup(MacroAssembler* masm,
                                             Register object,
                                             Register key,
                                             Register scratch1,
                                             Register scratch2,
                                             Label* unmapped_case,
                                             Label* slow_case) {
  Heap* heap = masm->isolate()->heap();
  Factory* factory = masm->isolate()->factory();

  // Check that the receiver is a JSObject. Because of the elements
  // map check later, we do not need to check for interceptors or
  // whether it requires access checks.
  __ JumpIfSmi(object, slow_case);
  // Check that the object is some kind of JSObject.
  __ CmpObjectType(object, FIRST_JS_RECEIVER_TYPE, scratch1);
  __ j(below, slow_case);

  // Check that the key is a positive smi.
  __ test(key, Immediate(0x80000001));
  __ j(not_zero, slow_case);

  // Load the elements into scratch1 and check its map.
  Handle<Map> arguments_map(heap->sloppy_arguments_elements_map());
  __ mov(scratch1, FieldOperand(object, JSObject::kElementsOffset));
  __ CheckMap(scratch1, arguments_map, slow_case, DONT_DO_SMI_CHECK);

  // Check if element is in the range of mapped arguments. If not, jump
  // to the unmapped lookup with the parameter map in scratch1.
  __ mov(scratch2, FieldOperand(scratch1, FixedArray::kLengthOffset));
  __ sub(scratch2, Immediate(Smi::FromInt(2)));
  __ cmp(key, scratch2);
  __ j(above_equal, unmapped_case);

  // Load element index and check whether it is the hole.
  const int kHeaderSize = FixedArray::kHeaderSize + 2 * kPointerSize;
  __ mov(scratch2, FieldOperand(scratch1,
                                key,
                                times_half_pointer_size,
                                kHeaderSize));
  __ cmp(scratch2, factory->the_hole_value());
  __ j(equal, unmapped_case);

  // Load value from context and return it. We can reuse scratch1 because
  // we do not jump to the unmapped lookup (which requires the parameter
  // map in scratch1).
  const int kContextOffset = FixedArray::kHeaderSize;
  __ mov(scratch1, FieldOperand(scratch1, kContextOffset));
  return FieldOperand(scratch1,
                      scratch2,
                      times_half_pointer_size,
                      Context::kHeaderSize);
}


static Operand GenerateUnmappedArgumentsLookup(MacroAssembler* masm,
                                               Register key,
                                               Register parameter_map,
                                               Register scratch,
                                               Label* slow_case) {
  // Element is in arguments backing store, which is referenced by the
  // second element of the parameter_map.
  const int kBackingStoreOffset = FixedArray::kHeaderSize + kPointerSize;
  Register backing_store = parameter_map;
  __ mov(backing_store, FieldOperand(parameter_map, kBackingStoreOffset));
  Handle<Map> fixed_array_map(masm->isolate()->heap()->fixed_array_map());
  __ CheckMap(backing_store, fixed_array_map, slow_case, DONT_DO_SMI_CHECK);
  __ mov(scratch, FieldOperand(backing_store, FixedArray::kLengthOffset));
  __ cmp(key, scratch);
  __ j(greater_equal, slow_case);
  return FieldOperand(backing_store,
                      key,
                      times_half_pointer_size,
                      FixedArray::kHeaderSize);
}


void KeyedLoadIC::GenerateGeneric(MacroAssembler* masm) {
  // The return address is on the stack.
  Label slow, check_name, index_smi, index_name, property_array_property;
  Label probe_dictionary, check_number_dictionary;

  Register receiver = ReceiverRegister();
  Register key = NameRegister();
  DCHECK(receiver.is(edx));
  DCHECK(key.is(ecx));

  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &check_name);
  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from
  // where a numeric string is converted to a smi.

  GenerateKeyedLoadReceiverCheck(
      masm, receiver, eax, Map::kHasIndexedInterceptor, &slow);

  // Check the receiver's map to see if it has fast elements.
  __ CheckFastElements(eax, &check_number_dictionary);

  GenerateFastArrayLoad(masm, receiver, key, eax, eax, NULL, &slow);
  Isolate* isolate = masm->isolate();
  Counters* counters = isolate->counters();
  __ IncrementCounter(counters->keyed_load_generic_smi(), 1);
  __ ret(0);

  __ bind(&check_number_dictionary);
  __ mov(ebx, key);
  __ SmiUntag(ebx);
  __ mov(eax, FieldOperand(receiver, JSObject::kElementsOffset));

  // Check whether the elements is a number dictionary.
  // ebx: untagged index
  // eax: elements
  __ CheckMap(eax,
              isolate->factory()->hash_table_map(),
              &slow,
              DONT_DO_SMI_CHECK);
  Label slow_pop_receiver;
  // Push receiver on the stack to free up a register for the dictionary
  // probing.
  __ push(receiver);
  __ LoadFromNumberDictionary(&slow_pop_receiver, eax, key, ebx, edx, edi, eax);
  // Pop receiver before returning.
  __ pop(receiver);
  __ ret(0);

  __ bind(&slow_pop_receiver);
  // Pop the receiver from the stack and jump to runtime.
  __ pop(receiver);

  __ bind(&slow);
  // Slow case: jump to runtime.
  __ IncrementCounter(counters->keyed_load_generic_slow(), 1);
  GenerateRuntimeGetProperty(masm);

  __ bind(&check_name);
  GenerateKeyNameCheck(masm, key, eax, ebx, &index_name, &slow);

  GenerateKeyedLoadReceiverCheck(
      masm, receiver, eax, Map::kHasNamedInterceptor, &slow);

  // If the receiver is a fast-case object, check the keyed lookup
  // cache. Otherwise probe the dictionary.
  __ mov(ebx, FieldOperand(receiver, JSObject::kPropertiesOffset));
  __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
         Immediate(isolate->factory()->hash_table_map()));
  __ j(equal, &probe_dictionary);

  // The receiver's map is still in eax, compute the keyed lookup cache hash
  // based on 32 bits of the map pointer and the string hash.
  if (FLAG_debug_code) {
    __ cmp(eax, FieldOperand(receiver, HeapObject::kMapOffset));
    __ Check(equal, kMapIsNoLongerInEax);
  }
  __ mov(ebx, eax);  // Keep the map around for later.
  __ shr(eax, KeyedLookupCache::kMapHashShift);
  __ mov(edi, FieldOperand(key, String::kHashFieldOffset));
  __ shr(edi, String::kHashShift);
  __ xor_(eax, edi);
  __ and_(eax, KeyedLookupCache::kCapacityMask & KeyedLookupCache::kHashMask);

  // Load the key (consisting of map and internalized string) from the cache and
  // check for match.
  Label load_in_object_property;
  static const int kEntriesPerBucket = KeyedLookupCache::kEntriesPerBucket;
  Label hit_on_nth_entry[kEntriesPerBucket];
  ExternalReference cache_keys =
      ExternalReference::keyed_lookup_cache_keys(masm->isolate());

  for (int i = 0; i < kEntriesPerBucket - 1; i++) {
    Label try_next_entry;
    __ mov(edi, eax);
    __ shl(edi, kPointerSizeLog2 + 1);
    if (i != 0) {
      __ add(edi, Immediate(kPointerSize * i * 2));
    }
    __ cmp(ebx, Operand::StaticArray(edi, times_1, cache_keys));
    __ j(not_equal, &try_next_entry);
    __ add(edi, Immediate(kPointerSize));
    __ cmp(key, Operand::StaticArray(edi, times_1, cache_keys));
    __ j(equal, &hit_on_nth_entry[i]);
    __ bind(&try_next_entry);
  }

  __ lea(edi, Operand(eax, 1));
  __ shl(edi, kPointerSizeLog2 + 1);
  __ add(edi, Immediate(kPointerSize * (kEntriesPerBucket - 1) * 2));
  __ cmp(ebx, Operand::StaticArray(edi, times_1, cache_keys));
  __ j(not_equal, &slow);
  __ add(edi, Immediate(kPointerSize));
  __ cmp(key, Operand::StaticArray(edi, times_1, cache_keys));
  __ j(not_equal, &slow);

  // Get field offset.
  // ebx      : receiver's map
  // eax      : lookup cache index
  ExternalReference cache_field_offsets =
      ExternalReference::keyed_lookup_cache_field_offsets(masm->isolate());

  // Hit on nth entry.
  for (int i = kEntriesPerBucket - 1; i >= 0; i--) {
    __ bind(&hit_on_nth_entry[i]);
    if (i != 0) {
      __ add(eax, Immediate(i));
    }
    __ mov(edi,
           Operand::StaticArray(eax, times_pointer_size, cache_field_offsets));
    __ movzx_b(eax, FieldOperand(ebx, Map::kInObjectPropertiesOffset));
    __ sub(edi, eax);
    __ j(above_equal, &property_array_property);
    if (i != 0) {
      __ jmp(&load_in_object_property);
    }
  }

  // Load in-object property.
  __ bind(&load_in_object_property);
  __ movzx_b(eax, FieldOperand(ebx, Map::kInstanceSizeOffset));
  __ add(eax, edi);
  __ mov(eax, FieldOperand(receiver, eax, times_pointer_size, 0));
  __ IncrementCounter(counters->keyed_load_generic_lookup_cache(), 1);
  __ ret(0);

  // Load property array property.
  __ bind(&property_array_property);
  __ mov(eax, FieldOperand(receiver, JSObject::kPropertiesOffset));
  __ mov(eax, FieldOperand(eax, edi, times_pointer_size,
                           FixedArray::kHeaderSize));
  __ IncrementCounter(counters->keyed_load_generic_lookup_cache(), 1);
  __ ret(0);

  // Do a quick inline probe of the receiver's dictionary, if it
  // exists.
  __ bind(&probe_dictionary);

  __ mov(eax, FieldOperand(receiver, JSObject::kMapOffset));
  __ movzx_b(eax, FieldOperand(eax, Map::kInstanceTypeOffset));
  GenerateGlobalInstanceTypeCheck(masm, eax, &slow);

  GenerateDictionaryLoad(masm, &slow, ebx, key, eax, edi, eax);
  __ IncrementCounter(counters->keyed_load_generic_symbol(), 1);
  __ ret(0);

  __ bind(&index_name);
  __ IndexFromHash(ebx, key);
  // Now jump to the place where smi keys are handled.
  __ jmp(&index_smi);
}


void KeyedLoadIC::GenerateString(MacroAssembler* masm) {
  // Return address is on the stack.
  Label miss;

  Register receiver = ReceiverRegister();
  Register index = NameRegister();
  Register scratch = ebx;
  DCHECK(!scratch.is(receiver) && !scratch.is(index));
  Register result = eax;
  DCHECK(!result.is(scratch));

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
  GenerateMiss(masm);
}


void KeyedLoadIC::GenerateIndexedInterceptor(MacroAssembler* masm) {
  // Return address is on the stack.
  Label slow;

  Register receiver = ReceiverRegister();
  Register key = NameRegister();
  Register scratch = eax;
  DCHECK(!scratch.is(receiver) && !scratch.is(key));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &slow);

  // Check that the key is an array index, that is Uint32.
  __ test(key, Immediate(kSmiTagMask | kSmiSignMask));
  __ j(not_zero, &slow);

  // Get the map of the receiver.
  __ mov(scratch, FieldOperand(receiver, HeapObject::kMapOffset));

  // Check that it has indexed interceptor and access checks
  // are not enabled for this object.
  __ movzx_b(scratch, FieldOperand(scratch, Map::kBitFieldOffset));
  __ and_(scratch, Immediate(kSlowCaseBitFieldMask));
  __ cmp(scratch, Immediate(1 << Map::kHasIndexedInterceptor));
  __ j(not_zero, &slow);

  // Everything is fine, call runtime.
  __ pop(scratch);
  __ push(receiver);  // receiver
  __ push(key);       // key
  __ push(scratch);   // return address

  // Perform tail call to the entry.
  ExternalReference ref = ExternalReference(
      IC_Utility(kLoadElementWithInterceptor), masm->isolate());
  __ TailCallExternalReference(ref, 2, 1);

  __ bind(&slow);
  GenerateMiss(masm);
}


void KeyedLoadIC::GenerateSloppyArguments(MacroAssembler* masm) {
  // The return address is on the stack.
  Register receiver = ReceiverRegister();
  Register key = NameRegister();
  DCHECK(receiver.is(edx));
  DCHECK(key.is(ecx));

  Label slow, notin;
  Factory* factory = masm->isolate()->factory();
  Operand mapped_location =
      GenerateMappedArgumentsLookup(
          masm, receiver, key, ebx, eax, &notin, &slow);
  __ mov(eax, mapped_location);
  __ Ret();
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in ebx.
  Operand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, key, ebx, eax, &slow);
  __ cmp(unmapped_location, factory->the_hole_value());
  __ j(equal, &slow);
  __ mov(eax, unmapped_location);
  __ Ret();
  __ bind(&slow);
  GenerateMiss(masm);
}


void KeyedStoreIC::GenerateSloppyArguments(MacroAssembler* masm) {
  // Return address is on the stack.
  Label slow, notin;
  Register receiver = ReceiverRegister();
  Register name = NameRegister();
  Register value = ValueRegister();
  DCHECK(receiver.is(edx));
  DCHECK(name.is(ecx));
  DCHECK(value.is(eax));

  Operand mapped_location =
      GenerateMappedArgumentsLookup(masm, receiver, name, ebx, edi, &notin,
                                    &slow);
  __ mov(mapped_location, value);
  __ lea(ecx, mapped_location);
  __ mov(edx, value);
  __ RecordWrite(ebx, ecx, edx, kDontSaveFPRegs);
  __ Ret();
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in ebx.
  Operand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, name, ebx, edi, &slow);
  __ mov(unmapped_location, value);
  __ lea(edi, unmapped_location);
  __ mov(edx, value);
  __ RecordWrite(ebx, edi, edx, kDontSaveFPRegs);
  __ Ret();
  __ bind(&slow);
  GenerateMiss(masm);
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
  Register receiver = KeyedStoreIC::ReceiverRegister();
  Register key = KeyedStoreIC::NameRegister();
  Register value = KeyedStoreIC::ValueRegister();
  DCHECK(receiver.is(edx));
  DCHECK(key.is(ecx));
  DCHECK(value.is(eax));
  // key is a smi.
  // ebx: FixedArray receiver->elements
  // edi: receiver map
  // Fast case: Do the store, could either Object or double.
  __ bind(fast_object);
  if (check_map == kCheckMap) {
    __ mov(edi, FieldOperand(ebx, HeapObject::kMapOffset));
    __ cmp(edi, masm->isolate()->factory()->fixed_array_map());
    __ j(not_equal, fast_double);
  }

  // HOLECHECK: guards "A[i] = V"
  // We have to go to the runtime if the current value is the hole because
  // there may be a callback on the element
  Label holecheck_passed1;
  __ cmp(FixedArrayElementOperand(ebx, key),
         masm->isolate()->factory()->the_hole_value());
  __ j(not_equal, &holecheck_passed1);
  __ JumpIfDictionaryInPrototypeChain(receiver, ebx, edi, slow);
  __ mov(ebx, FieldOperand(receiver, JSObject::kElementsOffset));

  __ bind(&holecheck_passed1);

  // Smi stores don't require further checks.
  Label non_smi_value;
  __ JumpIfNotSmi(value, &non_smi_value);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ add(FieldOperand(receiver, JSArray::kLengthOffset),
           Immediate(Smi::FromInt(1)));
  }
  // It's irrelevant whether array is smi-only or not when writing a smi.
  __ mov(FixedArrayElementOperand(ebx, key), value);
  __ ret(0);

  __ bind(&non_smi_value);
  // Escape to elements kind transition case.
  __ mov(edi, FieldOperand(receiver, HeapObject::kMapOffset));
  __ CheckFastObjectElements(edi, &transition_smi_elements);

  // Fast elements array, store the value to the elements backing store.
  __ bind(&finish_object_store);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ add(FieldOperand(receiver, JSArray::kLengthOffset),
           Immediate(Smi::FromInt(1)));
  }
  __ mov(FixedArrayElementOperand(ebx, key), value);
  // Update write barrier for the elements array address.
  __ mov(edx, value);  // Preserve the value which is returned.
  __ RecordWriteArray(
      ebx, edx, key, kDontSaveFPRegs, EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  __ ret(0);

  __ bind(fast_double);
  if (check_map == kCheckMap) {
    // Check for fast double array case. If this fails, call through to the
    // runtime.
    __ cmp(edi, masm->isolate()->factory()->fixed_double_array_map());
    __ j(not_equal, slow);
    // If the value is a number, store it as a double in the FastDoubleElements
    // array.
  }

  // HOLECHECK: guards "A[i] double hole?"
  // We have to see if the double version of the hole is present. If so
  // go to the runtime.
  uint32_t offset = FixedDoubleArray::kHeaderSize + sizeof(kHoleNanLower32);
  __ cmp(FieldOperand(ebx, key, times_4, offset), Immediate(kHoleNanUpper32));
  __ j(not_equal, &fast_double_without_map_check);
  __ JumpIfDictionaryInPrototypeChain(receiver, ebx, edi, slow);
  __ mov(ebx, FieldOperand(receiver, JSObject::kElementsOffset));

  __ bind(&fast_double_without_map_check);
  __ StoreNumberToDoubleElements(value, ebx, key, edi, xmm0,
                                 &transition_double_elements);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ add(FieldOperand(receiver, JSArray::kLengthOffset),
           Immediate(Smi::FromInt(1)));
  }
  __ ret(0);

  __ bind(&transition_smi_elements);
  __ mov(ebx, FieldOperand(receiver, HeapObject::kMapOffset));

  // Transition the array appropriately depending on the value type.
  __ CheckMap(value,
              masm->isolate()->factory()->heap_number_map(),
              &non_double_value,
              DONT_DO_SMI_CHECK);

  // Value is a double. Transition FAST_SMI_ELEMENTS -> FAST_DOUBLE_ELEMENTS
  // and complete the store.
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                         FAST_DOUBLE_ELEMENTS,
                                         ebx,
                                         edi,
                                         slow);
  AllocationSiteMode mode = AllocationSite::GetMode(FAST_SMI_ELEMENTS,
                                                    FAST_DOUBLE_ELEMENTS);
  ElementsTransitionGenerator::GenerateSmiToDouble(
      masm, receiver, key, value, ebx, mode, slow);
  __ mov(ebx, FieldOperand(receiver, JSObject::kElementsOffset));
  __ jmp(&fast_double_without_map_check);

  __ bind(&non_double_value);
  // Value is not a double, FAST_SMI_ELEMENTS -> FAST_ELEMENTS
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                         FAST_ELEMENTS,
                                         ebx,
                                         edi,
                                         slow);
  mode = AllocationSite::GetMode(FAST_SMI_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
      masm, receiver, key, value, ebx, mode, slow);
  __ mov(ebx, FieldOperand(receiver, JSObject::kElementsOffset));
  __ jmp(&finish_object_store);

  __ bind(&transition_double_elements);
  // Elements are FAST_DOUBLE_ELEMENTS, but value is an Object that's not a
  // HeapNumber. Make sure that the receiver is a Array with FAST_ELEMENTS and
  // transition array from FAST_DOUBLE_ELEMENTS to FAST_ELEMENTS
  __ mov(ebx, FieldOperand(receiver, HeapObject::kMapOffset));
  __ LoadTransitionedArrayMapConditional(FAST_DOUBLE_ELEMENTS,
                                         FAST_ELEMENTS,
                                         ebx,
                                         edi,
                                         slow);
  mode = AllocationSite::GetMode(FAST_DOUBLE_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateDoubleToObject(
      masm, receiver, key, value, ebx, mode, slow);
  __ mov(ebx, FieldOperand(receiver, JSObject::kElementsOffset));
  __ jmp(&finish_object_store);
}


void KeyedStoreIC::GenerateGeneric(MacroAssembler* masm,
                                   StrictMode strict_mode) {
  // Return address is on the stack.
  Label slow, fast_object, fast_object_grow;
  Label fast_double, fast_double_grow;
  Label array, extra, check_if_double_array;
  Register receiver = ReceiverRegister();
  Register key = NameRegister();
  DCHECK(receiver.is(edx));
  DCHECK(key.is(ecx));

  // Check that the object isn't a smi.
  __ JumpIfSmi(receiver, &slow);
  // Get the map from the receiver.
  __ mov(edi, FieldOperand(receiver, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks and is not observed.
  // The generic stub does not perform map checks or handle observed objects.
  __ test_b(FieldOperand(edi, Map::kBitFieldOffset),
            1 << Map::kIsAccessCheckNeeded | 1 << Map::kIsObserved);
  __ j(not_zero, &slow);
  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &slow);
  __ CmpInstanceType(edi, JS_ARRAY_TYPE);
  __ j(equal, &array);
  // Check that the object is some kind of JSObject.
  __ CmpInstanceType(edi, FIRST_JS_OBJECT_TYPE);
  __ j(below, &slow);

  // Object case: Check key against length in the elements array.
  // Key is a smi.
  // edi: receiver map
  __ mov(ebx, FieldOperand(receiver, JSObject::kElementsOffset));
  // Check array bounds. Both the key and the length of FixedArray are smis.
  __ cmp(key, FieldOperand(ebx, FixedArray::kLengthOffset));
  __ j(below, &fast_object);

  // Slow case: call runtime.
  __ bind(&slow);
  GenerateRuntimeSetProperty(masm, strict_mode);

  // Extra capacity case: Check if there is extra capacity to
  // perform the store and update the length. Used for adding one
  // element to the array by writing to array[array.length].
  __ bind(&extra);
  // receiver is a JSArray.
  // key is a smi.
  // ebx: receiver->elements, a FixedArray
  // edi: receiver map
  // flags: compare (key, receiver.length())
  // do not leave holes in the array:
  __ j(not_equal, &slow);
  __ cmp(key, FieldOperand(ebx, FixedArray::kLengthOffset));
  __ j(above_equal, &slow);
  __ mov(edi, FieldOperand(ebx, HeapObject::kMapOffset));
  __ cmp(edi, masm->isolate()->factory()->fixed_array_map());
  __ j(not_equal, &check_if_double_array);
  __ jmp(&fast_object_grow);

  __ bind(&check_if_double_array);
  __ cmp(edi, masm->isolate()->factory()->fixed_double_array_map());
  __ j(not_equal, &slow);
  __ jmp(&fast_double_grow);

  // Array case: Get the length and the elements array from the JS
  // array. Check that the array is in fast mode (and writable); if it
  // is the length is always a smi.
  __ bind(&array);
  // receiver is a JSArray.
  // key is a smi.
  // edi: receiver map
  __ mov(ebx, FieldOperand(receiver, JSObject::kElementsOffset));

  // Check the key against the length in the array and fall through to the
  // common store code.
  __ cmp(key, FieldOperand(receiver, JSArray::kLengthOffset));  // Compare smis.
  __ j(above_equal, &extra);

  KeyedStoreGenerateGenericHelper(masm, &fast_object, &fast_double,
                                  &slow, kCheckMap, kDontIncrementLength);
  KeyedStoreGenerateGenericHelper(masm, &fast_object_grow, &fast_double_grow,
                                  &slow, kDontCheckMap, kIncrementLength);
}


void LoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  // The return address is on the stack.
  Register receiver = ReceiverRegister();
  Register name = NameRegister();
  DCHECK(receiver.is(edx));
  DCHECK(name.is(ecx));

  // Probe the stub cache.
  Code::Flags flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::LOAD_IC));
  masm->isolate()->stub_cache()->GenerateProbe(
      masm, flags, receiver, name, ebx, eax);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
}


void LoadIC::GenerateNormal(MacroAssembler* masm) {
  Register dictionary = eax;
  DCHECK(!dictionary.is(ReceiverRegister()));
  DCHECK(!dictionary.is(NameRegister()));

  Label slow;

  __ mov(dictionary,
         FieldOperand(ReceiverRegister(), JSObject::kPropertiesOffset));
  GenerateDictionaryLoad(masm, &slow, dictionary, NameRegister(), edi, ebx,
                         eax);
  __ ret(0);

  // Dictionary load failed, go slow (but don't miss).
  __ bind(&slow);
  GenerateRuntimeGetProperty(masm);
}


static void LoadIC_PushArgs(MacroAssembler* masm) {
  Register receiver = LoadIC::ReceiverRegister();
  Register name = LoadIC::NameRegister();
  DCHECK(!ebx.is(receiver) && !ebx.is(name));

  __ pop(ebx);
  __ push(receiver);
  __ push(name);
  __ push(ebx);
}


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // Return address is on the stack.
  __ IncrementCounter(masm->isolate()->counters()->load_miss(), 1);

  LoadIC_PushArgs(masm);

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kLoadIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 2, 1);
}


void LoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // Return address is on the stack.
  LoadIC_PushArgs(masm);

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kGetProperty, 2, 1);
}


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  // Return address is on the stack.
  __ IncrementCounter(masm->isolate()->counters()->keyed_load_miss(), 1);

  LoadIC_PushArgs(masm);

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kKeyedLoadIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 2, 1);
}


// IC register specifications
const Register LoadIC::ReceiverRegister() { return edx; }
const Register LoadIC::NameRegister() { return ecx; }


const Register LoadIC::SlotRegister() {
  DCHECK(FLAG_vector_ics);
  return eax;
}


const Register LoadIC::VectorRegister() {
  DCHECK(FLAG_vector_ics);
  return ebx;
}


const Register StoreIC::ReceiverRegister() { return edx; }
const Register StoreIC::NameRegister() { return ecx; }
const Register StoreIC::ValueRegister() { return eax; }


const Register KeyedStoreIC::MapRegister() {
  return ebx;
}


void KeyedLoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // Return address is on the stack.
  LoadIC_PushArgs(masm);

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kKeyedGetProperty, 2, 1);
}


void StoreIC::GenerateMegamorphic(MacroAssembler* masm) {
  // Return address is on the stack.
  Code::Flags flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::STORE_IC));
  masm->isolate()->stub_cache()->GenerateProbe(
      masm, flags, ReceiverRegister(), NameRegister(),
      ebx, no_reg);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
}


static void StoreIC_PushArgs(MacroAssembler* masm) {
  Register receiver = StoreIC::ReceiverRegister();
  Register name = StoreIC::NameRegister();
  Register value = StoreIC::ValueRegister();

  DCHECK(!ebx.is(receiver) && !ebx.is(name) && !ebx.is(value));

  __ pop(ebx);
  __ push(receiver);
  __ push(name);
  __ push(value);
  __ push(ebx);
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
  Label restore_miss;
  Register receiver = ReceiverRegister();
  Register name = NameRegister();
  Register value = ValueRegister();
  Register dictionary = ebx;

  __ mov(dictionary, FieldOperand(receiver, JSObject::kPropertiesOffset));

  // A lot of registers are needed for storing to slow case
  // objects. Push and restore receiver but rely on
  // GenerateDictionaryStore preserving the value and name.
  __ push(receiver);
  GenerateDictionaryStore(masm, &restore_miss, dictionary, name, value,
                          receiver, edi);
  __ Drop(1);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->store_normal_hit(), 1);
  __ ret(0);

  __ bind(&restore_miss);
  __ pop(receiver);
  __ IncrementCounter(counters->store_normal_miss(), 1);
  GenerateMiss(masm);
}


void StoreIC::GenerateRuntimeSetProperty(MacroAssembler* masm,
                                         StrictMode strict_mode) {
  // Return address is on the stack.
  DCHECK(!ebx.is(ReceiverRegister()) && !ebx.is(NameRegister()) &&
         !ebx.is(ValueRegister()));
  __ pop(ebx);
  __ push(ReceiverRegister());
  __ push(NameRegister());
  __ push(ValueRegister());
  __ push(Immediate(Smi::FromInt(strict_mode)));
  __ push(ebx);  // return address

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty, 4, 1);
}


void KeyedStoreIC::GenerateRuntimeSetProperty(MacroAssembler* masm,
                                              StrictMode strict_mode) {
  // Return address is on the stack.
  DCHECK(!ebx.is(ReceiverRegister()) && !ebx.is(NameRegister()) &&
         !ebx.is(ValueRegister()));
  __ pop(ebx);
  __ push(ReceiverRegister());
  __ push(NameRegister());
  __ push(ValueRegister());
  __ push(Immediate(Smi::FromInt(strict_mode)));
  __ push(ebx);  // return address

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty, 4, 1);
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm) {
  // Return address is on the stack.
  StoreIC_PushArgs(masm);

  // Do tail-call to runtime routine.
  ExternalReference ref =
      ExternalReference(IC_Utility(kKeyedStoreIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void StoreIC::GenerateSlow(MacroAssembler* masm) {
  // Return address is on the stack.
  StoreIC_PushArgs(masm);

  // Do tail-call to runtime routine.
  ExternalReference ref(IC_Utility(kStoreIC_Slow), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void KeyedStoreIC::GenerateSlow(MacroAssembler* masm) {
  // Return address is on the stack.
  StoreIC_PushArgs(masm);

  // Do tail-call to runtime routine.
  ExternalReference ref(IC_Utility(kKeyedStoreIC_Slow), masm->isolate());
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
    PrintF("[  patching ic at %p, test=%p, delta=%d\n",
           address, test_instruction_address, delta);
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
  Condition cc = (check == ENABLE_INLINED_SMI_CHECK)
      ? (*jmp_address == Assembler::kJncShortOpcode ? not_zero : zero)
      : (*jmp_address == Assembler::kJnzShortOpcode ? not_carry : carry);
  *jmp_address = static_cast<byte>(Assembler::kJccShortPrefix | cc);
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
