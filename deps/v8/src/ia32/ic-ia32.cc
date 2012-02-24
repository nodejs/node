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

#if defined(V8_TARGET_ARCH_IA32)

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
  __ cmp(type, JS_GLOBAL_OBJECT_TYPE);
  __ j(equal, global_object);
  __ cmp(type, JS_BUILTINS_OBJECT_TYPE);
  __ j(equal, global_object);
  __ cmp(type, JS_GLOBAL_PROXY_TYPE);
  __ j(equal, global_object);
}


// Generated code falls through if the receiver is a regular non-global
// JS object with slow properties and no interceptors.
static void GenerateStringDictionaryReceiverCheck(MacroAssembler* masm,
                                                  Register receiver,
                                                  Register r0,
                                                  Register r1,
                                                  Label* miss) {
  // Register usage:
  //   receiver: holds the receiver on entry and is unchanged.
  //   r0: used to hold receiver instance type.
  //       Holds the property dictionary on fall through.
  //   r1: used to hold receivers map.

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // Check that the receiver is a valid JS object.
  __ mov(r1, FieldOperand(receiver, HeapObject::kMapOffset));
  __ movzx_b(r0, FieldOperand(r1, Map::kInstanceTypeOffset));
  __ cmp(r0, FIRST_SPEC_OBJECT_TYPE);
  __ j(below, miss);

  // If this assert fails, we have to check upper bound too.
  STATIC_ASSERT(LAST_TYPE == LAST_SPEC_OBJECT_TYPE);

  GenerateGlobalInstanceTypeCheck(masm, r0, miss);

  // Check for non-global object that requires access check.
  __ test_b(FieldOperand(r1, Map::kBitFieldOffset),
            (1 << Map::kIsAccessCheckNeeded) |
            (1 << Map::kHasNamedInterceptor));
  __ j(not_zero, miss);

  __ mov(r0, FieldOperand(receiver, JSObject::kPropertiesOffset));
  __ CheckMap(r0, FACTORY->hash_table_map(), miss, DONT_DO_SMI_CHECK);
}


// Helper function used to load a property from a dictionary backing
// storage. This function may fail to load a property even though it is
// in the dictionary, so code at miss_label must always call a backup
// property load that is complete. This function is safe to call if
// name is not a symbol, and will jump to the miss_label in that
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
  StringDictionaryLookupStub::GeneratePositiveLookup(masm,
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
      StringDictionary::kHeaderSize +
      StringDictionary::kElementsStartIndex * kPointerSize;
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
// call if name is not a symbol, and will jump to the miss_label in
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
  StringDictionaryLookupStub::GeneratePositiveLookup(masm,
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
      StringDictionary::kHeaderSize +
      StringDictionary::kElementsStartIndex * kPointerSize;
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


void LoadIC::GenerateArrayLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  StubCompiler::GenerateLoadArrayLength(masm, eax, edx, &miss);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateStringLength(MacroAssembler* masm,
                                  bool support_wrappers) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  StubCompiler::GenerateLoadStringLength(masm, eax, edx, ebx, &miss,
                                         support_wrappers);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateFunctionPrototype(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  StubCompiler::GenerateLoadFunctionPrototype(masm, eax, edx, ebx, &miss);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
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
  ASSERT(JS_OBJECT_TYPE > JS_VALUE_TYPE);

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
                FACTORY->fixed_array_map(),
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
  __ cmp(scratch, Immediate(FACTORY->the_hole_value()));
  // In case the loaded value is the_hole we have to consult GetProperty
  // to ensure the prototype chain is searched.
  __ j(equal, out_of_range);
  if (!result.is(scratch)) {
    __ mov(result, scratch);
  }
}


// Checks whether a key is an array index string or a symbol string.
// Falls through if the key is a symbol.
static void GenerateKeyStringCheck(MacroAssembler* masm,
                                   Register key,
                                   Register map,
                                   Register hash,
                                   Label* index_string,
                                   Label* not_symbol) {
  // Register use:
  //   key - holds the key and is unchanged. Assumed to be non-smi.
  // Scratch registers:
  //   map - used to hold the map of the key.
  //   hash - used to hold the hash of the key.
  __ CmpObjectType(key, FIRST_NONSTRING_TYPE, map);
  __ j(above_equal, not_symbol);

  // Is the string an array index, with cached numeric value?
  __ mov(hash, FieldOperand(key, String::kHashFieldOffset));
  __ test(hash, Immediate(String::kContainsCachedArrayIndexMask));
  __ j(zero, index_string);

  // Is the string a symbol?
  STATIC_ASSERT(kSymbolTag != 0);
  __ test_b(FieldOperand(map, Map::kInstanceTypeOffset), kIsSymbolMask);
  __ j(zero, not_symbol);
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
  __ test(key, Immediate(0x8000001));
  __ j(not_zero, slow_case);

  // Load the elements into scratch1 and check its map.
  Handle<Map> arguments_map(heap->non_strict_arguments_elements_map());
  __ mov(scratch1, FieldOperand(object, JSObject::kElementsOffset));
  __ CheckMap(scratch1, arguments_map, slow_case, DONT_DO_SMI_CHECK);

  // Check if element is in the range of mapped arguments. If not, jump
  // to the unmapped lookup with the parameter map in scratch1.
  __ mov(scratch2, FieldOperand(scratch1, FixedArray::kLengthOffset));
  __ sub(scratch2, Immediate(Smi::FromInt(2)));
  __ cmp(key, scratch2);
  __ j(greater_equal, unmapped_case);

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
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label slow, check_string, index_smi, index_string, property_array_property;
  Label probe_dictionary, check_number_dictionary;

  // Check that the key is a smi.
  __ JumpIfNotSmi(eax, &check_string);
  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from
  // where a numeric string is converted to a smi.

  GenerateKeyedLoadReceiverCheck(
      masm, edx, ecx, Map::kHasIndexedInterceptor, &slow);

  // Check the receiver's map to see if it has fast elements.
  __ CheckFastElements(ecx, &check_number_dictionary);

  GenerateFastArrayLoad(masm,
                        edx,
                        eax,
                        ecx,
                        eax,
                        NULL,
                        &slow);
  Isolate* isolate = masm->isolate();
  Counters* counters = isolate->counters();
  __ IncrementCounter(counters->keyed_load_generic_smi(), 1);
  __ ret(0);
  __ bind(&check_number_dictionary);
  __ mov(ebx, eax);
  __ SmiUntag(ebx);
  __ mov(ecx, FieldOperand(edx, JSObject::kElementsOffset));

  // Check whether the elements is a number dictionary.
  // edx: receiver
  // ebx: untagged index
  // eax: key
  // ecx: elements
  __ CheckMap(ecx,
              isolate->factory()->hash_table_map(),
              &slow,
              DONT_DO_SMI_CHECK);
  Label slow_pop_receiver;
  // Push receiver on the stack to free up a register for the dictionary
  // probing.
  __ push(edx);
  __ LoadFromNumberDictionary(&slow_pop_receiver,
                              ecx,
                              eax,
                              ebx,
                              edx,
                              edi,
                              eax);
  // Pop receiver before returning.
  __ pop(edx);
  __ ret(0);

  __ bind(&slow_pop_receiver);
  // Pop the receiver from the stack and jump to runtime.
  __ pop(edx);

  __ bind(&slow);
  // Slow case: jump to runtime.
  // edx: receiver
  // eax: key
  __ IncrementCounter(counters->keyed_load_generic_slow(), 1);
  GenerateRuntimeGetProperty(masm);

  __ bind(&check_string);
  GenerateKeyStringCheck(masm, eax, ecx, ebx, &index_string, &slow);

  GenerateKeyedLoadReceiverCheck(
      masm, edx, ecx, Map::kHasNamedInterceptor, &slow);

  // If the receiver is a fast-case object, check the keyed lookup
  // cache. Otherwise probe the dictionary.
  __ mov(ebx, FieldOperand(edx, JSObject::kPropertiesOffset));
  __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
         Immediate(isolate->factory()->hash_table_map()));
  __ j(equal, &probe_dictionary);

  // Load the map of the receiver, compute the keyed lookup cache hash
  // based on 32 bits of the map pointer and the string hash.
  __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));
  __ mov(ecx, ebx);
  __ shr(ecx, KeyedLookupCache::kMapHashShift);
  __ mov(edi, FieldOperand(eax, String::kHashFieldOffset));
  __ shr(edi, String::kHashShift);
  __ xor_(ecx, edi);
  __ and_(ecx, KeyedLookupCache::kCapacityMask & KeyedLookupCache::kHashMask);

  // Load the key (consisting of map and symbol) from the cache and
  // check for match.
  Label load_in_object_property;
  static const int kEntriesPerBucket = KeyedLookupCache::kEntriesPerBucket;
  Label hit_on_nth_entry[kEntriesPerBucket];
  ExternalReference cache_keys =
      ExternalReference::keyed_lookup_cache_keys(masm->isolate());

  for (int i = 0; i < kEntriesPerBucket - 1; i++) {
    Label try_next_entry;
    __ mov(edi, ecx);
    __ shl(edi, kPointerSizeLog2 + 1);
    if (i != 0) {
      __ add(edi, Immediate(kPointerSize * i * 2));
    }
    __ cmp(ebx, Operand::StaticArray(edi, times_1, cache_keys));
    __ j(not_equal, &try_next_entry);
    __ add(edi, Immediate(kPointerSize));
    __ cmp(eax, Operand::StaticArray(edi, times_1, cache_keys));
    __ j(equal, &hit_on_nth_entry[i]);
    __ bind(&try_next_entry);
  }

  __ lea(edi, Operand(ecx, 1));
  __ shl(edi, kPointerSizeLog2 + 1);
  __ add(edi, Immediate(kPointerSize * (kEntriesPerBucket - 1) * 2));
  __ cmp(ebx, Operand::StaticArray(edi, times_1, cache_keys));
  __ j(not_equal, &slow);
  __ add(edi, Immediate(kPointerSize));
  __ cmp(eax, Operand::StaticArray(edi, times_1, cache_keys));
  __ j(not_equal, &slow);

  // Get field offset.
  // edx     : receiver
  // ebx     : receiver's map
  // eax     : key
  // ecx     : lookup cache index
  ExternalReference cache_field_offsets =
      ExternalReference::keyed_lookup_cache_field_offsets(masm->isolate());

  // Hit on nth entry.
  for (int i = kEntriesPerBucket - 1; i >= 0; i--) {
    __ bind(&hit_on_nth_entry[i]);
    if (i != 0) {
      __ add(ecx, Immediate(i));
    }
    __ mov(edi,
           Operand::StaticArray(ecx, times_pointer_size, cache_field_offsets));
    __ movzx_b(ecx, FieldOperand(ebx, Map::kInObjectPropertiesOffset));
    __ sub(edi, ecx);
    __ j(above_equal, &property_array_property);
    if (i != 0) {
      __ jmp(&load_in_object_property);
    }
  }

  // Load in-object property.
  __ bind(&load_in_object_property);
  __ movzx_b(ecx, FieldOperand(ebx, Map::kInstanceSizeOffset));
  __ add(ecx, edi);
  __ mov(eax, FieldOperand(edx, ecx, times_pointer_size, 0));
  __ IncrementCounter(counters->keyed_load_generic_lookup_cache(), 1);
  __ ret(0);

  // Load property array property.
  __ bind(&property_array_property);
  __ mov(eax, FieldOperand(edx, JSObject::kPropertiesOffset));
  __ mov(eax, FieldOperand(eax, edi, times_pointer_size,
                           FixedArray::kHeaderSize));
  __ IncrementCounter(counters->keyed_load_generic_lookup_cache(), 1);
  __ ret(0);

  // Do a quick inline probe of the receiver's dictionary, if it
  // exists.
  __ bind(&probe_dictionary);

  __ mov(ecx, FieldOperand(edx, JSObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  GenerateGlobalInstanceTypeCheck(masm, ecx, &slow);

  GenerateDictionaryLoad(masm, &slow, ebx, eax, ecx, edi, eax);
  __ IncrementCounter(counters->keyed_load_generic_symbol(), 1);
  __ ret(0);

  __ bind(&index_string);
  __ IndexFromHash(ebx, eax);
  // Now jump to the place where smi keys are handled.
  __ jmp(&index_smi);
}


void KeyedLoadIC::GenerateString(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : key (index)
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  Register receiver = edx;
  Register index = eax;
  Register scratch = ecx;
  Register result = eax;

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
  GenerateMiss(masm, false);
}


void KeyedLoadIC::GenerateIndexedInterceptor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label slow;

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(edx, &slow);

  // Check that the key is an array index, that is Uint32.
  __ test(eax, Immediate(kSmiTagMask | kSmiSignMask));
  __ j(not_zero, &slow);

  // Get the map of the receiver.
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));

  // Check that it has indexed interceptor and access checks
  // are not enabled for this object.
  __ movzx_b(ecx, FieldOperand(ecx, Map::kBitFieldOffset));
  __ and_(ecx, Immediate(kSlowCaseBitFieldMask));
  __ cmp(ecx, Immediate(1 << Map::kHasIndexedInterceptor));
  __ j(not_zero, &slow);

  // Everything is fine, call runtime.
  __ pop(ecx);
  __ push(edx);  // receiver
  __ push(eax);  // key
  __ push(ecx);  // return address

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kKeyedLoadPropertyWithInterceptor),
                        masm->isolate());
  __ TailCallExternalReference(ref, 2, 1);

  __ bind(&slow);
  GenerateMiss(masm, false);
}


void KeyedLoadIC::GenerateNonStrictArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label slow, notin;
  Factory* factory = masm->isolate()->factory();
  Operand mapped_location =
      GenerateMappedArgumentsLookup(masm, edx, eax, ebx, ecx, &notin, &slow);
  __ mov(eax, mapped_location);
  __ Ret();
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in ebx.
  Operand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, eax, ebx, ecx, &slow);
  __ cmp(unmapped_location, factory->the_hole_value());
  __ j(equal, &slow);
  __ mov(eax, unmapped_location);
  __ Ret();
  __ bind(&slow);
  GenerateMiss(masm, false);
}


void KeyedStoreIC::GenerateNonStrictArguments(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label slow, notin;
  Operand mapped_location =
      GenerateMappedArgumentsLookup(masm, edx, ecx, ebx, edi, &notin, &slow);
  __ mov(mapped_location, eax);
  __ lea(ecx, mapped_location);
  __ mov(edx, eax);
  __ RecordWrite(ebx, ecx, edx, kDontSaveFPRegs);
  __ Ret();
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in ebx.
  Operand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, ecx, ebx, edi, &slow);
  __ mov(unmapped_location, eax);
  __ lea(edi, unmapped_location);
  __ mov(edx, eax);
  __ RecordWrite(ebx, edi, edx, kDontSaveFPRegs);
  __ Ret();
  __ bind(&slow);
  GenerateMiss(masm, false);
}


void KeyedStoreIC::GenerateGeneric(MacroAssembler* masm,
                                   StrictModeFlag strict_mode) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label slow, fast_object_with_map_check, fast_object_without_map_check;
  Label fast_double_with_map_check, fast_double_without_map_check;
  Label check_if_double_array, array, extra, transition_smi_elements;
  Label finish_object_store, non_double_value, transition_double_elements;

  // Check that the object isn't a smi.
  __ JumpIfSmi(edx, &slow);
  // Get the map from the receiver.
  __ mov(edi, FieldOperand(edx, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.  We need
  // to do this because this generic stub does not perform map checks.
  __ test_b(FieldOperand(edi, Map::kBitFieldOffset),
            1 << Map::kIsAccessCheckNeeded);
  __ j(not_zero, &slow);
  // Check that the key is a smi.
  __ JumpIfNotSmi(ecx, &slow);
  __ CmpInstanceType(edi, JS_ARRAY_TYPE);
  __ j(equal, &array);
  // Check that the object is some kind of JSObject.
  __ CmpInstanceType(edi, FIRST_JS_OBJECT_TYPE);
  __ j(below, &slow);

  // Object case: Check key against length in the elements array.
  // eax: value
  // edx: JSObject
  // ecx: key (a smi)
  // edi: receiver map
  __ mov(ebx, FieldOperand(edx, JSObject::kElementsOffset));
  // Check array bounds. Both the key and the length of FixedArray are smis.
  __ cmp(ecx, FieldOperand(ebx, FixedArray::kLengthOffset));
  __ j(below, &fast_object_with_map_check);

  // Slow case: call runtime.
  __ bind(&slow);
  GenerateRuntimeSetProperty(masm, strict_mode);

  // Extra capacity case: Check if there is extra capacity to
  // perform the store and update the length. Used for adding one
  // element to the array by writing to array[array.length].
  __ bind(&extra);
  // eax: value
  // edx: receiver, a JSArray
  // ecx: key, a smi.
  // ebx: receiver->elements, a FixedArray
  // edi: receiver map
  // flags: compare (ecx, edx.length())
  // do not leave holes in the array:
  __ j(not_equal, &slow);
  __ cmp(ecx, FieldOperand(ebx, FixedArray::kLengthOffset));
  __ j(above_equal, &slow);
  __ mov(edi, FieldOperand(ebx, HeapObject::kMapOffset));
  __ cmp(edi, masm->isolate()->factory()->fixed_array_map());
  __ j(not_equal, &check_if_double_array);
  // Add 1 to receiver->length, and go to common element store code for Objects.
  __ add(FieldOperand(edx, JSArray::kLengthOffset),
         Immediate(Smi::FromInt(1)));
  __ jmp(&fast_object_without_map_check);

  __ bind(&check_if_double_array);
  __ cmp(edi, masm->isolate()->factory()->fixed_double_array_map());
  __ j(not_equal, &slow);
  // Add 1 to receiver->length, and go to common element store code for doubles.
  __ add(FieldOperand(edx, JSArray::kLengthOffset),
         Immediate(Smi::FromInt(1)));
  __ jmp(&fast_double_without_map_check);

  // Array case: Get the length and the elements array from the JS
  // array. Check that the array is in fast mode (and writable); if it
  // is the length is always a smi.
  __ bind(&array);
  // eax: value
  // edx: receiver, a JSArray
  // ecx: key, a smi.
  // edi: receiver map
  __ mov(ebx, FieldOperand(edx, JSObject::kElementsOffset));

  // Check the key against the length in the array and fall through to the
  // common store code.
  __ cmp(ecx, FieldOperand(edx, JSArray::kLengthOffset));  // Compare smis.
  __ j(above_equal, &extra);

  // Fast case: Do the store, could either Object or double.
  __ bind(&fast_object_with_map_check);
  // eax: value
  // ecx: key (a smi)
  // edx: receiver
  // ebx: FixedArray receiver->elements
  // edi: receiver map
  __ mov(edi, FieldOperand(ebx, HeapObject::kMapOffset));
  __ cmp(edi, masm->isolate()->factory()->fixed_array_map());
  __ j(not_equal, &fast_double_with_map_check);
  __ bind(&fast_object_without_map_check);
  // Smi stores don't require further checks.
  Label non_smi_value;
  __ JumpIfNotSmi(eax, &non_smi_value);
  // It's irrelevant whether array is smi-only or not when writing a smi.
  __ mov(CodeGenerator::FixedArrayElementOperand(ebx, ecx), eax);
  __ ret(0);

  __ bind(&non_smi_value);
  // Escape to elements kind transition case.
  __ mov(edi, FieldOperand(edx, HeapObject::kMapOffset));
  __ CheckFastObjectElements(edi, &transition_smi_elements);

  // Fast elements array, store the value to the elements backing store.
  __ bind(&finish_object_store);
  __ mov(CodeGenerator::FixedArrayElementOperand(ebx, ecx), eax);
  // Update write barrier for the elements array address.
  __ mov(edx, eax);  // Preserve the value which is returned.
  __ RecordWriteArray(
      ebx, edx, ecx, kDontSaveFPRegs, EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  __ ret(0);

  __ bind(&fast_double_with_map_check);
  // Check for fast double array case. If this fails, call through to the
  // runtime.
  __ cmp(edi, masm->isolate()->factory()->fixed_double_array_map());
  __ j(not_equal, &slow);
  __ bind(&fast_double_without_map_check);
  // If the value is a number, store it as a double in the FastDoubleElements
  // array.
  __ StoreNumberToDoubleElements(eax, ebx, ecx, edx, xmm0,
                                 &transition_double_elements, false);
  __ ret(0);

  __ bind(&transition_smi_elements);
  __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));

  // Transition the array appropriately depending on the value type.
  __ CheckMap(eax,
              masm->isolate()->factory()->heap_number_map(),
              &non_double_value,
              DONT_DO_SMI_CHECK);

  // Value is a double. Transition FAST_SMI_ONLY_ELEMENTS ->
  // FAST_DOUBLE_ELEMENTS and complete the store.
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ONLY_ELEMENTS,
                                         FAST_DOUBLE_ELEMENTS,
                                         ebx,
                                         edi,
                                         &slow);
  ElementsTransitionGenerator::GenerateSmiOnlyToDouble(masm, &slow);
  __ mov(ebx, FieldOperand(edx, JSObject::kElementsOffset));
  __ jmp(&fast_double_without_map_check);

  __ bind(&non_double_value);
  // Value is not a double, FAST_SMI_ONLY_ELEMENTS -> FAST_ELEMENTS
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ONLY_ELEMENTS,
                                         FAST_ELEMENTS,
                                         ebx,
                                         edi,
                                         &slow);
  ElementsTransitionGenerator::GenerateSmiOnlyToObject(masm);
  __ mov(ebx, FieldOperand(edx, JSObject::kElementsOffset));
  __ jmp(&finish_object_store);

  __ bind(&transition_double_elements);
  // Elements are FAST_DOUBLE_ELEMENTS, but value is an Object that's not a
  // HeapNumber. Make sure that the receiver is a Array with FAST_ELEMENTS and
  // transition array from FAST_DOUBLE_ELEMENTS to FAST_ELEMENTS
  __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));
  __ LoadTransitionedArrayMapConditional(FAST_DOUBLE_ELEMENTS,
                                         FAST_ELEMENTS,
                                         ebx,
                                         edi,
                                         &slow);
  ElementsTransitionGenerator::GenerateDoubleToObject(masm, &slow);
  __ mov(ebx, FieldOperand(edx, JSObject::kElementsOffset));
  __ jmp(&finish_object_store);
}


// The generated code does not accept smi keys.
// The generated code falls through if both probes miss.
void CallICBase::GenerateMonomorphicCacheProbe(MacroAssembler* masm,
                                               int argc,
                                               Code::Kind kind,
                                               Code::ExtraICState extra_state) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- edx                 : receiver
  // -----------------------------------
  Label number, non_number, non_string, boolean, probe, miss;

  // Probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(kind,
                                         MONOMORPHIC,
                                         extra_state,
                                         NORMAL,
                                         argc);
  Isolate* isolate = masm->isolate();
  isolate->stub_cache()->GenerateProbe(masm, flags, edx, ecx, ebx, eax);

  // If the stub cache probing failed, the receiver might be a value.
  // For value objects, we use the map of the prototype objects for
  // the corresponding JSValue for the cache and that is what we need
  // to probe.
  //
  // Check for number.
  __ JumpIfSmi(edx, &number);
  __ CmpObjectType(edx, HEAP_NUMBER_TYPE, ebx);
  __ j(not_equal, &non_number);
  __ bind(&number);
  StubCompiler::GenerateLoadGlobalFunctionPrototype(
      masm, Context::NUMBER_FUNCTION_INDEX, edx);
  __ jmp(&probe);

  // Check for string.
  __ bind(&non_number);
  __ CmpInstanceType(ebx, FIRST_NONSTRING_TYPE);
  __ j(above_equal, &non_string);
  StubCompiler::GenerateLoadGlobalFunctionPrototype(
      masm, Context::STRING_FUNCTION_INDEX, edx);
  __ jmp(&probe);

  // Check for boolean.
  __ bind(&non_string);
  __ cmp(edx, isolate->factory()->true_value());
  __ j(equal, &boolean);
  __ cmp(edx, isolate->factory()->false_value());
  __ j(not_equal, &miss);
  __ bind(&boolean);
  StubCompiler::GenerateLoadGlobalFunctionPrototype(
      masm, Context::BOOLEAN_FUNCTION_INDEX, edx);

  // Probe the stub cache for the value object.
  __ bind(&probe);
  isolate->stub_cache()->GenerateProbe(masm, flags, edx, ecx, ebx, no_reg);
  __ bind(&miss);
}


static void GenerateFunctionTailCall(MacroAssembler* masm,
                                     int argc,
                                     Label* miss) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- edi                 : function
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // Check that the result is not a smi.
  __ JumpIfSmi(edi, miss);

  // Check that the value is a JavaScript function, fetching its map into eax.
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, eax);
  __ j(not_equal, miss);

  // Invoke the function.
  ParameterCount actual(argc);
  __ InvokeFunction(edi, actual, JUMP_FUNCTION,
                    NullCallWrapper(), CALL_AS_METHOD);
}


// The generated code falls through if the call should be handled by runtime.
void CallICBase::GenerateNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------
  Label miss;

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  GenerateStringDictionaryReceiverCheck(masm, edx, eax, ebx, &miss);

  // eax: elements
  // Search the dictionary placing the result in edi.
  GenerateDictionaryLoad(masm, &miss, eax, ecx, edi, ebx, edi);
  GenerateFunctionTailCall(masm, argc, &miss);

  __ bind(&miss);
}


void CallICBase::GenerateMiss(MacroAssembler* masm,
                              int argc,
                              IC::UtilityId id,
                              Code::ExtraICState extra_state) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  Counters* counters = masm->isolate()->counters();
  if (id == IC::kCallIC_Miss) {
    __ IncrementCounter(counters->call_miss(), 1);
  } else {
    __ IncrementCounter(counters->keyed_call_miss(), 1);
  }

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  {
    FrameScope scope(masm, StackFrame::INTERNAL);

    // Push the receiver and the name of the function.
    __ push(edx);
    __ push(ecx);

    // Call the entry.
    CEntryStub stub(1);
    __ mov(eax, Immediate(2));
    __ mov(ebx, Immediate(ExternalReference(IC_Utility(id), masm->isolate())));
    __ CallStub(&stub);

    // Move result to edi and exit the internal frame.
    __ mov(edi, eax);
  }

  // Check if the receiver is a global object of some sort.
  // This can happen only for regular CallIC but not KeyedCallIC.
  if (id == IC::kCallIC_Miss) {
    Label invoke, global;
    __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));  // receiver
    __ JumpIfSmi(edx, &invoke, Label::kNear);
    __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));
    __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
    __ cmp(ebx, JS_GLOBAL_OBJECT_TYPE);
    __ j(equal, &global, Label::kNear);
    __ cmp(ebx, JS_BUILTINS_OBJECT_TYPE);
    __ j(not_equal, &invoke, Label::kNear);

    // Patch the receiver on the stack.
    __ bind(&global);
    __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
    __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
    __ bind(&invoke);
  }

  // Invoke the function.
  CallKind call_kind = CallICBase::Contextual::decode(extra_state)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  ParameterCount actual(argc);
  __ InvokeFunction(edi,
                    actual,
                    JUMP_FUNCTION,
                    NullCallWrapper(),
                    call_kind);
}


void CallIC::GenerateMegamorphic(MacroAssembler* masm,
                                 int argc,
                                 Code::ExtraICState extra_state) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));
  CallICBase::GenerateMonomorphicCacheProbe(masm, argc, Code::CALL_IC,
                                            extra_state);

  GenerateMiss(masm, argc, extra_state);
}


void KeyedCallIC::GenerateMegamorphic(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  Label do_call, slow_call, slow_load, slow_reload_receiver;
  Label check_number_dictionary, check_string, lookup_monomorphic_cache;
  Label index_smi, index_string;

  // Check that the key is a smi.
  __ JumpIfNotSmi(ecx, &check_string);

  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from
  // where a numeric string is converted to a smi.

  GenerateKeyedLoadReceiverCheck(
      masm, edx, eax, Map::kHasIndexedInterceptor, &slow_call);

  GenerateFastArrayLoad(
      masm, edx, ecx, eax, edi, &check_number_dictionary, &slow_load);
  Isolate* isolate = masm->isolate();
  Counters* counters = isolate->counters();
  __ IncrementCounter(counters->keyed_call_generic_smi_fast(), 1);

  __ bind(&do_call);
  // receiver in edx is not used after this point.
  // ecx: key
  // edi: function
  GenerateFunctionTailCall(masm, argc, &slow_call);

  __ bind(&check_number_dictionary);
  // eax: elements
  // ecx: smi key
  // Check whether the elements is a number dictionary.
  __ CheckMap(eax,
              isolate->factory()->hash_table_map(),
              &slow_load,
              DONT_DO_SMI_CHECK);
  __ mov(ebx, ecx);
  __ SmiUntag(ebx);
  // ebx: untagged index
  // Receiver in edx will be clobbered, need to reload it on miss.
  __ LoadFromNumberDictionary(
      &slow_reload_receiver, eax, ecx, ebx, edx, edi, edi);
  __ IncrementCounter(counters->keyed_call_generic_smi_dict(), 1);
  __ jmp(&do_call);

  __ bind(&slow_reload_receiver);
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  __ bind(&slow_load);
  // This branch is taken when calling KeyedCallIC_Miss is neither required
  // nor beneficial.
  __ IncrementCounter(counters->keyed_call_generic_slow_load(), 1);

  {
    FrameScope scope(masm, StackFrame::INTERNAL);
    __ push(ecx);  // save the key
    __ push(edx);  // pass the receiver
    __ push(ecx);  // pass the key
    __ CallRuntime(Runtime::kKeyedGetProperty, 2);
    __ pop(ecx);  // restore the key
    // Leave the internal frame.
  }

  __ mov(edi, eax);
  __ jmp(&do_call);

  __ bind(&check_string);
  GenerateKeyStringCheck(masm, ecx, eax, ebx, &index_string, &slow_call);

  // The key is known to be a symbol.
  // If the receiver is a regular JS object with slow properties then do
  // a quick inline probe of the receiver's dictionary.
  // Otherwise do the monomorphic cache probe.
  GenerateKeyedLoadReceiverCheck(
      masm, edx, eax, Map::kHasNamedInterceptor, &lookup_monomorphic_cache);

  __ mov(ebx, FieldOperand(edx, JSObject::kPropertiesOffset));
  __ CheckMap(ebx,
              isolate->factory()->hash_table_map(),
              &lookup_monomorphic_cache,
              DONT_DO_SMI_CHECK);

  GenerateDictionaryLoad(masm, &slow_load, ebx, ecx, eax, edi, edi);
  __ IncrementCounter(counters->keyed_call_generic_lookup_dict(), 1);
  __ jmp(&do_call);

  __ bind(&lookup_monomorphic_cache);
  __ IncrementCounter(counters->keyed_call_generic_lookup_cache(), 1);
  CallICBase::GenerateMonomorphicCacheProbe(masm, argc, Code::KEYED_CALL_IC,
                                            Code::kNoExtraICState);
  // Fall through on miss.

  __ bind(&slow_call);
  // This branch is taken if:
  // - the receiver requires boxing or access check,
  // - the key is neither smi nor symbol,
  // - the value loaded is not a function,
  // - there is hope that the runtime will create a monomorphic call stub
  //   that will get fetched next time.
  __ IncrementCounter(counters->keyed_call_generic_slow(), 1);
  GenerateMiss(masm, argc);

  __ bind(&index_string);
  __ IndexFromHash(ebx, ecx);
  // Now jump to the place where smi keys are handled.
  __ jmp(&index_smi);
}


void KeyedCallIC::GenerateNonStrictArguments(MacroAssembler* masm,
                                             int argc) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------
  Label slow, notin;
  Factory* factory = masm->isolate()->factory();
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));
  Operand mapped_location =
      GenerateMappedArgumentsLookup(masm, edx, ecx, ebx, eax, &notin, &slow);
  __ mov(edi, mapped_location);
  GenerateFunctionTailCall(masm, argc, &slow);
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in ebx.
  Operand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, ecx, ebx, eax, &slow);
  __ cmp(unmapped_location, factory->the_hole_value());
  __ j(equal, &slow);
  __ mov(edi, unmapped_location);
  GenerateFunctionTailCall(masm, argc, &slow);
  __ bind(&slow);
  GenerateMiss(masm, argc);
}


void KeyedCallIC::GenerateNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // Check if the name is a string.
  Label miss;
  __ JumpIfSmi(ecx, &miss);
  Condition cond = masm->IsObjectStringType(ecx, eax, eax);
  __ j(NegateCondition(cond), &miss);
  CallICBase::GenerateNormal(masm, argc);
  __ bind(&miss);
  GenerateMiss(masm, argc);
}


void LoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------

  // Probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(Code::LOAD_IC, MONOMORPHIC);
  Isolate::Current()->stub_cache()->GenerateProbe(masm, flags, eax, ecx, ebx,
                                                  edx);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
}


void LoadIC::GenerateNormal(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  GenerateStringDictionaryReceiverCheck(masm, eax, edx, ebx, &miss);

  // edx: elements
  // Search the dictionary placing the result in eax.
  GenerateDictionaryLoad(masm, &miss, edx, ecx, edi, ebx, eax);
  __ ret(0);

  // Cache miss: Jump to runtime.
  __ bind(&miss);
  GenerateMiss(masm);
}


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------

  __ IncrementCounter(masm->isolate()->counters()->load_miss(), 1);

  __ pop(ebx);
  __ push(eax);  // receiver
  __ push(ecx);  // name
  __ push(ebx);  // return address

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kLoadIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 2, 1);
}


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm, bool force_generic) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  __ IncrementCounter(masm->isolate()->counters()->keyed_load_miss(), 1);

  __ pop(ebx);
  __ push(edx);  // receiver
  __ push(eax);  // name
  __ push(ebx);  // return address

  // Perform tail call to the entry.
  ExternalReference ref = force_generic
      ? ExternalReference(IC_Utility(kKeyedLoadIC_MissForceGeneric),
                          masm->isolate())
      : ExternalReference(IC_Utility(kKeyedLoadIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 2, 1);
}


void KeyedLoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  __ pop(ebx);
  __ push(edx);  // receiver
  __ push(eax);  // name
  __ push(ebx);  // return address

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kKeyedGetProperty, 2, 1);
}


void StoreIC::GenerateMegamorphic(MacroAssembler* masm,
                                  StrictModeFlag strict_mode) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  Code::Flags flags =
      Code::ComputeFlags(Code::STORE_IC, MONOMORPHIC, strict_mode);
  Isolate::Current()->stub_cache()->GenerateProbe(masm, flags, edx, ecx, ebx,
                                                  no_reg);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
}


void StoreIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  __ pop(ebx);
  __ push(edx);
  __ push(ecx);
  __ push(eax);
  __ push(ebx);

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kStoreIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void StoreIC::GenerateArrayLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  //
  // This accepts as a receiver anything JSArray::SetElementsLength accepts
  // (currently anything except for external arrays which means anything with
  // elements of FixedArray type).  Value must be a number, but only smis are
  // accepted as the most common case.

  Label miss;

  Register receiver = edx;
  Register value = eax;
  Register scratch = ebx;

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Check that the object is a JS array.
  __ CmpObjectType(receiver, JS_ARRAY_TYPE, scratch);
  __ j(not_equal, &miss);

  // Check that elements are FixedArray.
  // We rely on StoreIC_ArrayLength below to deal with all types of
  // fast elements (including COW).
  __ mov(scratch, FieldOperand(receiver, JSArray::kElementsOffset));
  __ CmpObjectType(scratch, FIXED_ARRAY_TYPE, scratch);
  __ j(not_equal, &miss);

  // Check that the array has fast properties, otherwise the length
  // property might have been redefined.
  __ mov(scratch, FieldOperand(receiver, JSArray::kPropertiesOffset));
  __ CompareRoot(FieldOperand(scratch, FixedArray::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(equal, &miss);

  // Check that value is a smi.
  __ JumpIfNotSmi(value, &miss);

  // Prepare tail call to StoreIC_ArrayLength.
  __ pop(scratch);
  __ push(receiver);
  __ push(value);
  __ push(scratch);  // return address

  ExternalReference ref =
      ExternalReference(IC_Utility(kStoreIC_ArrayLength), masm->isolate());
  __ TailCallExternalReference(ref, 2, 1);

  __ bind(&miss);

  GenerateMiss(masm);
}


void StoreIC::GenerateNormal(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  Label miss, restore_miss;

  GenerateStringDictionaryReceiverCheck(masm, edx, ebx, edi, &miss);

  // A lot of registers are needed for storing to slow case
  // objects. Push and restore receiver but rely on
  // GenerateDictionaryStore preserving the value and name.
  __ push(edx);
  GenerateDictionaryStore(masm, &restore_miss, ebx, ecx, eax, edx, edi);
  __ Drop(1);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->store_normal_hit(), 1);
  __ ret(0);

  __ bind(&restore_miss);
  __ pop(edx);

  __ bind(&miss);
  __ IncrementCounter(counters->store_normal_miss(), 1);
  GenerateMiss(masm);
}


void StoreIC::GenerateGlobalProxy(MacroAssembler* masm,
                                  StrictModeFlag strict_mode) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  __ pop(ebx);
  __ push(edx);
  __ push(ecx);
  __ push(eax);
  __ push(Immediate(Smi::FromInt(NONE)));  // PropertyAttributes
  __ push(Immediate(Smi::FromInt(strict_mode)));
  __ push(ebx);  // return address

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty, 5, 1);
}


void KeyedStoreIC::GenerateRuntimeSetProperty(MacroAssembler* masm,
                                              StrictModeFlag strict_mode) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  __ pop(ebx);
  __ push(edx);
  __ push(ecx);
  __ push(eax);
  __ push(Immediate(Smi::FromInt(NONE)));         // PropertyAttributes
  __ push(Immediate(Smi::FromInt(strict_mode)));  // Strict mode.
  __ push(ebx);   // return address

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty, 5, 1);
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm, bool force_generic) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  __ pop(ebx);
  __ push(edx);
  __ push(ecx);
  __ push(eax);
  __ push(ebx);

  // Do tail-call to runtime routine.
  ExternalReference ref = force_generic
      ? ExternalReference(IC_Utility(kKeyedStoreIC_MissForceGeneric),
                          masm->isolate())
      : ExternalReference(IC_Utility(kKeyedStoreIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void KeyedStoreIC::GenerateSlow(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  __ pop(ebx);
  __ push(edx);
  __ push(ecx);
  __ push(eax);
  __ push(ebx);   // return address

  // Do tail-call to runtime routine.
  ExternalReference ref(IC_Utility(kKeyedStoreIC_Slow), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void KeyedStoreIC::GenerateTransitionElementsSmiToDouble(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- ebx    : target map
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  // Must return the modified receiver in eax.
  if (!FLAG_trace_elements_transitions) {
    Label fail;
    ElementsTransitionGenerator::GenerateSmiOnlyToDouble(masm, &fail);
    __ mov(eax, edx);
    __ Ret();
    __ bind(&fail);
  }

  __ pop(ebx);
  __ push(edx);
  __ push(ebx);  // return address
  // Leaving the code managed by the register allocator and return to the
  // convention of using esi as context register.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  __ TailCallRuntime(Runtime::kTransitionElementsSmiToDouble, 1, 1);
}


void KeyedStoreIC::GenerateTransitionElementsDoubleToObject(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- ebx    : target map
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  // Must return the modified receiver in eax.
  if (!FLAG_trace_elements_transitions) {
    Label fail;
    ElementsTransitionGenerator::GenerateDoubleToObject(masm, &fail);
    __ mov(eax, edx);
    __ Ret();
    __ bind(&fail);
  }

  __ pop(ebx);
  __ push(edx);
  __ push(ebx);  // return address
  // Leaving the code managed by the register allocator and return to the
  // convention of using esi as context register.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  __ TailCallRuntime(Runtime::kTransitionElementsDoubleToObject, 1, 1);
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


static bool HasInlinedSmiCode(Address address) {
  // The address of the instruction following the call.
  Address test_instruction_address =
      address + Assembler::kCallTargetAddressOffset;

  // If the instruction following the call is not a test al, nothing
  // was inlined.
  return *test_instruction_address == Assembler::kTestAlByte;
}


void CompareIC::UpdateCaches(Handle<Object> x, Handle<Object> y) {
  HandleScope scope;
  Handle<Code> rewritten;
  State previous_state = GetState();

  State state = TargetState(previous_state, HasInlinedSmiCode(address()), x, y);
  if (state == GENERIC) {
    CompareStub stub(GetCondition(), strict(), NO_COMPARE_FLAGS);
    rewritten = stub.GetCode();
  } else {
    ICCompareStub stub(op_, state);
    if (state == KNOWN_OBJECTS) {
      stub.set_known_map(Handle<Map>(Handle<JSObject>::cast(x)->map()));
    }
    rewritten = stub.GetCode();
  }
  set_target(*rewritten);

#ifdef DEBUG
  if (FLAG_trace_ic) {
    PrintF("[CompareIC (%s->%s)#%s]\n",
           GetStateName(previous_state),
           GetStateName(state),
           Token::Name(op_));
  }
#endif

  // Activate inlined smi code.
  if (previous_state == UNINITIALIZED) {
    PatchInlinedSmiCode(address());
  }
}


void PatchInlinedSmiCode(Address address) {
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

  // Patch with a short conditional jump. There must be a
  // short jump-if-carry/not-carry at this position.
  Address jmp_address = test_instruction_address - delta;
  ASSERT(*jmp_address == Assembler::kJncShortOpcode ||
         *jmp_address == Assembler::kJcShortOpcode);
  Condition cc = *jmp_address == Assembler::kJncShortOpcode
      ? not_zero
      : zero;
  *jmp_address = static_cast<byte>(Assembler::kJccShortPrefix | cc);
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
