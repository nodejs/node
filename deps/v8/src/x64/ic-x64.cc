// Copyright 2010 the V8 project authors. All rights reserved.
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

#if defined(V8_TARGET_ARCH_X64)

#include "codegen-inl.h"
#include "ic-inl.h"
#include "runtime.h"
#include "stub-cache.h"
#include "utils.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Static IC stub generators.
//

#define __ ACCESS_MASM(masm)


// Helper function used to load a property from a dictionary backing storage.
// This function may return false negatives, so miss_label
// must always call a backup property load that is complete.
// This function is safe to call if the receiver has fast properties,
// or if name is not a symbol, and will jump to the miss_label in that case.
static void GenerateDictionaryLoad(MacroAssembler* masm,
                                   Label* miss_label,
                                   Register r0,
                                   Register r1,
                                   Register r2,
                                   Register name,
                                   Register r4,
                                   Register result,
                                   DictionaryCheck check_dictionary) {
  // Register use:
  //
  // r0   - used to hold the property dictionary and is unchanged.
  //
  // r1   - used to hold the receiver and is unchanged.
  //
  // r2   - used to hold the capacity of the property dictionary.
  //
  // name - holds the name of the property and is unchanged.
  //
  // r4   - used to hold the index into the property dictionary.
  //
  // result - holds the result on exit if the load succeeded.

  Label done;

  // Check for the absence of an interceptor.
  // Load the map into r0.
  __ movq(r0, FieldOperand(r1, JSObject::kMapOffset));

  // Bail out if the receiver has a named interceptor.
  __ testl(FieldOperand(r0, Map::kBitFieldOffset),
           Immediate(1 << Map::kHasNamedInterceptor));
  __ j(not_zero, miss_label);

  // Bail out if we have a JS global proxy object.
  __ movzxbq(r0, FieldOperand(r0, Map::kInstanceTypeOffset));
  __ cmpb(r0, Immediate(JS_GLOBAL_PROXY_TYPE));
  __ j(equal, miss_label);

  // Possible work-around for http://crbug.com/16276.
  __ cmpb(r0, Immediate(JS_GLOBAL_OBJECT_TYPE));
  __ j(equal, miss_label);
  __ cmpb(r0, Immediate(JS_BUILTINS_OBJECT_TYPE));
  __ j(equal, miss_label);

  // Load properties array.
  __ movq(r0, FieldOperand(r1, JSObject::kPropertiesOffset));

  if (check_dictionary == CHECK_DICTIONARY) {
    // Check that the properties array is a dictionary.
    __ Cmp(FieldOperand(r0, HeapObject::kMapOffset), Factory::hash_table_map());
    __ j(not_equal, miss_label);
  }

  // Compute the capacity mask.
  const int kCapacityOffset =
      StringDictionary::kHeaderSize +
      StringDictionary::kCapacityIndex * kPointerSize;
  __ SmiToInteger32(r2, FieldOperand(r0, kCapacityOffset));
  __ decl(r2);

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  static const int kProbes = 4;
  const int kElementsStartOffset =
      StringDictionary::kHeaderSize +
      StringDictionary::kElementsStartIndex * kPointerSize;
  for (int i = 0; i < kProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ movl(r4, FieldOperand(name, String::kHashFieldOffset));
    __ shrl(r4, Immediate(String::kHashShift));
    if (i > 0) {
      __ addl(r4, Immediate(StringDictionary::GetProbeOffset(i)));
    }
    __ and_(r4, r2);

    // Scale the index by multiplying by the entry size.
    ASSERT(StringDictionary::kEntrySize == 3);
    __ lea(r4, Operand(r4, r4, times_2, 0));  // r4 = r4 * 3

    // Check if the key is identical to the name.
    __ cmpq(name, Operand(r0, r4, times_pointer_size,
                          kElementsStartOffset - kHeapObjectTag));
    if (i != kProbes - 1) {
      __ j(equal, &done);
    } else {
      __ j(not_equal, miss_label);
    }
  }

  // Check that the value is a normal property.
  __ bind(&done);
  const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  __ Test(Operand(r0, r4, times_pointer_size, kDetailsOffset - kHeapObjectTag),
          Smi::FromInt(PropertyDetails::TypeField::mask()));
  __ j(not_zero, miss_label);

  // Get the value at the masked, scaled index.
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ movq(result,
          Operand(r0, r4, times_pointer_size, kValueOffset - kHeapObjectTag));
}


static void GenerateNumberDictionaryLoad(MacroAssembler* masm,
                                         Label* miss,
                                         Register elements,
                                         Register key,
                                         Register r0,
                                         Register r1,
                                         Register r2,
                                         Register result) {
  // Register use:
  //
  // elements - holds the slow-case elements of the receiver on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // key      - holds the smi key on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // Scratch registers:
  //
  // r0 - holds the untagged key on entry and holds the hash once computed.
  //
  // r1 - used to hold the capacity mask of the dictionary
  //
  // r2 - used for the index into the dictionary.
  //
  // result - holds the result on exit if the load succeeded.
  //          Allowed to be the same as 'key' or 'result'.
  //          Unchanged on bailout so 'key' or 'result' can be used
  //          in further computation.

  Label done;

  // Compute the hash code from the untagged key.  This must be kept in sync
  // with ComputeIntegerHash in utils.h.
  //
  // hash = ~hash + (hash << 15);
  __ movl(r1, r0);
  __ notl(r0);
  __ shll(r1, Immediate(15));
  __ addl(r0, r1);
  // hash = hash ^ (hash >> 12);
  __ movl(r1, r0);
  __ shrl(r1, Immediate(12));
  __ xorl(r0, r1);
  // hash = hash + (hash << 2);
  __ leal(r0, Operand(r0, r0, times_4, 0));
  // hash = hash ^ (hash >> 4);
  __ movl(r1, r0);
  __ shrl(r1, Immediate(4));
  __ xorl(r0, r1);
  // hash = hash * 2057;
  __ imull(r0, r0, Immediate(2057));
  // hash = hash ^ (hash >> 16);
  __ movl(r1, r0);
  __ shrl(r1, Immediate(16));
  __ xorl(r0, r1);

  // Compute capacity mask.
  __ SmiToInteger32(r1,
                    FieldOperand(elements, NumberDictionary::kCapacityOffset));
  __ decl(r1);

  // Generate an unrolled loop that performs a few probes before giving up.
  const int kProbes = 4;
  for (int i = 0; i < kProbes; i++) {
    // Use r2 for index calculations and keep the hash intact in r0.
    __ movq(r2, r0);
    // Compute the masked index: (hash + i + i * i) & mask.
    if (i > 0) {
      __ addl(r2, Immediate(NumberDictionary::GetProbeOffset(i)));
    }
    __ and_(r2, r1);

    // Scale the index by multiplying by the entry size.
    ASSERT(NumberDictionary::kEntrySize == 3);
    __ lea(r2, Operand(r2, r2, times_2, 0));  // r2 = r2 * 3

    // Check if the key matches.
    __ cmpq(key, FieldOperand(elements,
                              r2,
                              times_pointer_size,
                              NumberDictionary::kElementsStartOffset));
    if (i != (kProbes - 1)) {
      __ j(equal, &done);
    } else {
      __ j(not_equal, miss);
    }
  }

  __ bind(&done);
  // Check that the value is a normal propety.
  const int kDetailsOffset =
      NumberDictionary::kElementsStartOffset + 2 * kPointerSize;
  ASSERT_EQ(NORMAL, 0);
  __ Test(FieldOperand(elements, r2, times_pointer_size, kDetailsOffset),
          Smi::FromInt(PropertyDetails::TypeField::mask()));
  __ j(not_zero, miss);

  // Get the value at the masked, scaled index.
  const int kValueOffset =
      NumberDictionary::kElementsStartOffset + kPointerSize;
  __ movq(result, FieldOperand(elements, r2, times_pointer_size, kValueOffset));
}


// One byte opcode for test eax,0xXXXXXXXX.
static const byte kTestEaxByte = 0xA9;


static bool PatchInlinedMapCheck(Address address, Object* map) {
  // Arguments are address of start of call sequence that called
  // the IC,
  Address test_instruction_address =
      address + Assembler::kCallTargetAddressOffset;
  // The keyed load has a fast inlined case if the IC call instruction
  // is immediately followed by a test instruction.
  if (*test_instruction_address != kTestEaxByte) return false;

  // Fetch the offset from the test instruction to the map compare
  // instructions (starting with the 64-bit immediate mov of the map
  // address). This offset is stored in the last 4 bytes of the 5
  // byte test instruction.
  Address delta_address = test_instruction_address + 1;
  int delta = *reinterpret_cast<int*>(delta_address);
  // Compute the map address.  The map address is in the last 8 bytes
  // of the 10-byte immediate mov instruction (incl. REX prefix), so we add 2
  // to the offset to get the map address.
  Address map_address = test_instruction_address + delta + 2;
  // Patch the map check.
  *(reinterpret_cast<Object**>(map_address)) = map;
  return true;
}


bool KeyedLoadIC::PatchInlinedLoad(Address address, Object* map) {
  return PatchInlinedMapCheck(address, map);
}


bool KeyedStoreIC::PatchInlinedStore(Address address, Object* map) {
  return PatchInlinedMapCheck(address, map);
}


void KeyedLoadIC::ClearInlinedVersion(Address address) {
  // Insert null as the map to check for to make sure the map check fails
  // sending control flow to the IC instead of the inlined version.
  PatchInlinedLoad(address, Heap::null_value());
}


void KeyedStoreIC::ClearInlinedVersion(Address address) {
  // Insert null as the elements map to check for.  This will make
  // sure that the elements fast-case map check fails so that control
  // flows to the IC instead of the inlined version.
  PatchInlinedStore(address, Heap::null_value());
}


void KeyedStoreIC::RestoreInlinedVersion(Address address) {
  // Restore the fast-case elements map check so that the inlined
  // version can be used again.
  PatchInlinedStore(address, Heap::fixed_array_map());
}


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------

  __ pop(rbx);
  __ push(rdx);  // receiver
  __ push(rax);  // name
  __ push(rbx);  // return address

  // Perform tail call to the entry.
  ExternalReference ref = ExternalReference(IC_Utility(kKeyedLoadIC_Miss));
  __ TailCallExternalReference(ref, 2, 1);
}


void KeyedLoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------

  __ pop(rbx);
  __ push(rdx);  // receiver
  __ push(rax);  // name
  __ push(rbx);  // return address

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kKeyedGetProperty, 2, 1);
}


// Checks the receiver for special cases (value type, slow case bits).
// Falls through for regular JS object.
static void GenerateKeyedLoadReceiverCheck(MacroAssembler* masm,
                                           Register receiver,
                                           Register map,
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
           Immediate(KeyedLoadIC::kSlowCaseBitFieldMask));
  __ j(not_zero, slow);
}


// Loads an indexed element from a fast case array.
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
  // Check that the object is in fast mode (not dictionary).
  __ CompareRoot(FieldOperand(elements, HeapObject::kMapOffset),
                 Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, not_fast_array);
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
  __ movl(hash, FieldOperand(key, String::kHashFieldOffset));
  __ testl(hash, Immediate(String::kContainsCachedArrayIndexMask));
  __ j(zero, index_string);  // The value in hash is used at jump target.

  // Is the string a symbol?
  ASSERT(kSymbolTag != 0);
  __ testb(FieldOperand(map, Map::kInstanceTypeOffset),
           Immediate(kIsSymbolMask));
  __ j(zero, not_symbol);
}


// Picks out an array index from the hash field.
static void GenerateIndexFromHash(MacroAssembler* masm,
                                  Register key,
                                  Register hash) {
  // Register use:
  //   key - holds the overwritten key on exit.
  //   hash - holds the key's hash. Clobbered.

  // The assert checks that the constants for the maximum number of digits
  // for an array index cached in the hash field and the number of bits
  // reserved for it does not conflict.
  ASSERT(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  // We want the smi-tagged index in key. Even if we subsequently go to
  // the slow case, converting the key to a smi is always valid.
  // key: string key
  // hash: key's hash field, including its array index value.
  __ and_(hash, Immediate(String::kArrayIndexValueMask));
  __ shr(hash, Immediate(String::kHashShift));
  // Here we actually clobber the key which will be used if calling into
  // runtime later. However as the new key is the numeric value of a string key
  // there is no difference in using either key.
  __ Integer32ToSmi(key, hash);
}


void KeyedLoadIC::GenerateGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label slow, check_string, index_smi, index_string;
  Label check_pixel_array, probe_dictionary, check_number_dictionary;

  GenerateKeyedLoadReceiverCheck(masm, rdx, rcx, &slow);

  // Check that the key is a smi.
  __ JumpIfNotSmi(rax, &check_string);
  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from below
  // where a numeric string is converted to a smi.

  GenerateFastArrayLoad(masm,
                        rdx,
                        rax,
                        rcx,
                        rbx,
                        rax,
                        &check_pixel_array,
                        &slow);
  __ IncrementCounter(&Counters::keyed_load_generic_smi, 1);
  __ ret(0);

  __ bind(&check_pixel_array);
  // Check whether the elements object is a pixel array.
  // rdx: receiver
  // rax: key
  // rcx: elements array
  __ SmiToInteger32(rbx, rax);  // Used on both directions of next branch.
  __ CompareRoot(FieldOperand(rcx, HeapObject::kMapOffset),
                 Heap::kPixelArrayMapRootIndex);
  __ j(not_equal, &check_number_dictionary);
  __ cmpl(rbx, FieldOperand(rcx, PixelArray::kLengthOffset));
  __ j(above_equal, &slow);
  __ movq(rax, FieldOperand(rcx, PixelArray::kExternalPointerOffset));
  __ movzxbq(rax, Operand(rax, rbx, times_1, 0));
  __ Integer32ToSmi(rax, rax);
  __ ret(0);

  __ bind(&check_number_dictionary);
  // Check whether the elements is a number dictionary.
  // rdx: receiver
  // rax: key
  // rbx: key as untagged int32
  // rcx: elements
  __ CompareRoot(FieldOperand(rcx, HeapObject::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(not_equal, &slow);
  GenerateNumberDictionaryLoad(masm, &slow, rcx, rax, rbx, r9, rdi, rax);
  __ ret(0);

  __ bind(&slow);
  // Slow case: Jump to runtime.
  // rdx: receiver
  // rax: key
  __ IncrementCounter(&Counters::keyed_load_generic_slow, 1);
  GenerateRuntimeGetProperty(masm);

  __ bind(&check_string);
  GenerateKeyStringCheck(masm, rax, rcx, rbx, &index_string, &slow);

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
  __ and_(rcx, Immediate(KeyedLookupCache::kCapacityMask));

  // Load the key (consisting of map and symbol) from the cache and
  // check for match.
  ExternalReference cache_keys
      = ExternalReference::keyed_lookup_cache_keys();
  __ movq(rdi, rcx);
  __ shl(rdi, Immediate(kPointerSizeLog2 + 1));
  __ movq(kScratchRegister, cache_keys);
  __ cmpq(rbx, Operand(kScratchRegister, rdi, times_1, 0));
  __ j(not_equal, &slow);
  __ cmpq(rax, Operand(kScratchRegister, rdi, times_1, kPointerSize));
  __ j(not_equal, &slow);

  // Get field offset which is a 32-bit integer and check that it is
  // an in-object property.
  ExternalReference cache_field_offsets
      = ExternalReference::keyed_lookup_cache_field_offsets();
  __ movq(kScratchRegister, cache_field_offsets);
  __ movl(rdi, Operand(kScratchRegister, rcx, times_4, 0));
  __ movzxbq(rcx, FieldOperand(rbx, Map::kInObjectPropertiesOffset));
  __ subq(rdi, rcx);
  __ j(above_equal, &slow);

  // Load in-object property.
  __ movzxbq(rcx, FieldOperand(rbx, Map::kInstanceSizeOffset));
  __ addq(rcx, rdi);
  __ movq(rax, FieldOperand(rdx, rcx, times_pointer_size, 0));
  __ IncrementCounter(&Counters::keyed_load_generic_lookup_cache, 1);
  __ ret(0);

  // Do a quick inline probe of the receiver's dictionary, if it
  // exists.
  __ bind(&probe_dictionary);
  // rdx: receiver
  // rax: key
  GenerateDictionaryLoad(masm,
                         &slow,
                         rbx,
                         rdx,
                         rcx,
                         rax,
                         rdi,
                         rax,
                         DICTIONARY_CHECK_DONE);
  __ IncrementCounter(&Counters::keyed_load_generic_symbol, 1);
  __ ret(0);

  __ bind(&index_string);
  GenerateIndexFromHash(masm, rax, rbx);
  __ jmp(&index_smi);
}


void KeyedLoadIC::GenerateString(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;
  Label index_out_of_range;

  Register receiver = rdx;
  Register index = rax;
  Register scratch1 = rbx;
  Register scratch2 = rcx;
  Register result = rax;

  StringCharAtGenerator char_at_generator(receiver,
                                          index,
                                          scratch1,
                                          scratch2,
                                          result,
                                          &miss,  // When not a string.
                                          &miss,  // When not a number.
                                          &index_out_of_range,
                                          STRING_INDEX_IS_ARRAY_INDEX);
  char_at_generator.GenerateFast(masm);
  __ ret(0);

  ICRuntimeCallHelper call_helper;
  char_at_generator.GenerateSlow(masm, call_helper);

  __ bind(&index_out_of_range);
  __ LoadRoot(rax, Heap::kUndefinedValueRootIndex);
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void KeyedLoadIC::GenerateExternalArray(MacroAssembler* masm,
                                        ExternalArrayType array_type) {
  // ----------- S t a t e -------------
  //  -- rax    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label slow, failed_allocation;

  // Check that the object isn't a smi.
  __ JumpIfSmi(rdx, &slow);

  // Check that the key is a smi.
  __ JumpIfNotSmi(rax, &slow);

  // Check that the object is a JS object.
  __ CmpObjectType(rdx, JS_OBJECT_TYPE, rcx);
  __ j(not_equal, &slow);
  // Check that the receiver does not require access checks.  We need
  // to check this explicitly since this generic stub does not perform
  // map checks.  The map is already in rdx.
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &slow);

  // Check that the elements array is the appropriate type of
  // ExternalArray.
  // rax: index (as a smi)
  // rdx: JSObject
  __ movq(rbx, FieldOperand(rdx, JSObject::kElementsOffset));
  __ CompareRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                 Heap::RootIndexForExternalArrayType(array_type));
  __ j(not_equal, &slow);

  // Check that the index is in range.
  __ SmiToInteger32(rcx, rax);
  __ cmpl(rcx, FieldOperand(rbx, ExternalArray::kLengthOffset));
  // Unsigned comparison catches both negative and too-large values.
  __ j(above_equal, &slow);

  // rax: index (as a smi)
  // rdx: receiver (JSObject)
  // rcx: untagged index
  // rbx: elements array
  __ movq(rbx, FieldOperand(rbx, ExternalArray::kExternalPointerOffset));
  // rbx: base pointer of external storage
  switch (array_type) {
    case kExternalByteArray:
      __ movsxbq(rcx, Operand(rbx, rcx, times_1, 0));
      break;
    case kExternalUnsignedByteArray:
      __ movzxbq(rcx, Operand(rbx, rcx, times_1, 0));
      break;
    case kExternalShortArray:
      __ movsxwq(rcx, Operand(rbx, rcx, times_2, 0));
      break;
    case kExternalUnsignedShortArray:
      __ movzxwq(rcx, Operand(rbx, rcx, times_2, 0));
      break;
    case kExternalIntArray:
      __ movsxlq(rcx, Operand(rbx, rcx, times_4, 0));
      break;
    case kExternalUnsignedIntArray:
      __ movl(rcx, Operand(rbx, rcx, times_4, 0));
      break;
    case kExternalFloatArray:
      __ fld_s(Operand(rbx, rcx, times_4, 0));
      break;
    default:
      UNREACHABLE();
      break;
  }

  // rax: index
  // rdx: receiver
  // For integer array types:
  // rcx: value
  // For floating-point array type:
  // FP(0): value

  if (array_type == kExternalIntArray ||
      array_type == kExternalUnsignedIntArray) {
    // For the Int and UnsignedInt array types, we need to see whether
    // the value can be represented in a Smi. If not, we need to convert
    // it to a HeapNumber.
    Label box_int;
    if (array_type == kExternalIntArray) {
      __ JumpIfNotValidSmiValue(rcx, &box_int);
    } else {
      ASSERT_EQ(array_type, kExternalUnsignedIntArray);
      __ JumpIfUIntNotValidSmiValue(rcx, &box_int);
    }

    __ Integer32ToSmi(rax, rcx);
    __ ret(0);

    __ bind(&box_int);

    // Allocate a HeapNumber for the int and perform int-to-double
    // conversion.
    __ push(rcx);
    if (array_type == kExternalIntArray) {
      __ fild_s(Operand(rsp, 0));
    } else {
      ASSERT(array_type == kExternalUnsignedIntArray);
      // The value is zero-extended on the stack, because all pushes are
      // 64-bit and we loaded the value from memory with movl.
      __ fild_d(Operand(rsp, 0));
    }
    __ pop(rcx);
    // FP(0): value
    __ AllocateHeapNumber(rcx, rbx, &failed_allocation);
    // Set the value.
    __ movq(rax, rcx);
    __ fstp_d(FieldOperand(rax, HeapNumber::kValueOffset));
    __ ret(0);
  } else if (array_type == kExternalFloatArray) {
    // For the floating-point array type, we need to always allocate a
    // HeapNumber.
    __ AllocateHeapNumber(rcx, rbx, &failed_allocation);
    // Set the value.
    __ movq(rax, rcx);
    __ fstp_d(FieldOperand(rax, HeapNumber::kValueOffset));
    __ ret(0);
  } else {
    __ Integer32ToSmi(rax, rcx);
    __ ret(0);
  }

  // If we fail allocation of the HeapNumber, we still have a value on
  // top of the FPU stack. Remove it.
  __ bind(&failed_allocation);
  __ ffree();
  __ fincstp();
  // Fall through to slow case.

  // Slow case: Jump to runtime.
  __ bind(&slow);
  __ IncrementCounter(&Counters::keyed_load_external_array_slow, 1);
  GenerateRuntimeGetProperty(masm);
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

  // Check that the key is a smi.
  __ JumpIfNotSmi(rax, &slow);

  // Get the map of the receiver.
  __ movq(rcx, FieldOperand(rdx, HeapObject::kMapOffset));

  // Check that it has indexed interceptor and access checks
  // are not enabled for this object.
  __ movb(rcx, FieldOperand(rcx, Map::kBitFieldOffset));
  __ andb(rcx, Immediate(kSlowCaseBitFieldMask));
  __ cmpb(rcx, Immediate(1 << Map::kHasIndexedInterceptor));
  __ j(not_zero, &slow);

  // Everything is fine, call runtime.
  __ pop(rcx);
  __ push(rdx);  // receiver
  __ push(rax);  // key
  __ push(rcx);  // return address

  // Perform tail call to the entry.
  __ TailCallExternalReference(ExternalReference(
        IC_Utility(kKeyedLoadPropertyWithInterceptor)), 2, 1);

  __ bind(&slow);
  GenerateMiss(masm);
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : value
  //  -- rcx     : key
  //  -- rdx     : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------

  __ pop(rbx);
  __ push(rdx);  // receiver
  __ push(rcx);  // key
  __ push(rax);  // value
  __ push(rbx);  // return address

  // Do tail-call to runtime routine.
  ExternalReference ref = ExternalReference(IC_Utility(kKeyedStoreIC_Miss));
  __ TailCallExternalReference(ref, 3, 1);
}


void KeyedStoreIC::GenerateRuntimeSetProperty(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : value
  //  -- rcx     : key
  //  -- rdx     : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------

  __ pop(rbx);
  __ push(rdx);  // receiver
  __ push(rcx);  // key
  __ push(rax);  // value
  __ push(rbx);  // return address

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty, 3, 1);
}


void KeyedStoreIC::GenerateGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : value
  //  -- rcx     : key
  //  -- rdx     : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label slow, fast, array, extra, check_pixel_array;

  // Check that the object isn't a smi.
  __ JumpIfSmi(rdx, &slow);
  // Get the map from the receiver.
  __ movq(rbx, FieldOperand(rdx, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.  We need
  // to do this because this generic stub does not perform map checks.
  __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &slow);
  // Check that the key is a smi.
  __ JumpIfNotSmi(rcx, &slow);

  __ CmpInstanceType(rbx, JS_ARRAY_TYPE);
  __ j(equal, &array);
  // Check that the object is some kind of JS object.
  __ CmpInstanceType(rbx, FIRST_JS_OBJECT_TYPE);
  __ j(below, &slow);

  // Object case: Check key against length in the elements array.
  // rax: value
  // rdx: JSObject
  // rcx: index (as a smi)
  __ movq(rbx, FieldOperand(rdx, JSObject::kElementsOffset));
  // Check that the object is in fast mode (not dictionary).
  __ CompareRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                 Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &check_pixel_array);
  __ SmiCompare(rcx, FieldOperand(rbx, FixedArray::kLengthOffset));
  // rax: value
  // rbx: FixedArray
  // rcx: index (as a smi)
  __ j(below, &fast);

  // Slow case: call runtime.
  __ bind(&slow);
  GenerateRuntimeSetProperty(masm);

  // Check whether the elements is a pixel array.
  // rax: value
  // rdx: receiver
  // rbx: receiver's elements array
  // rcx: index (as a smi), zero-extended.
  __ bind(&check_pixel_array);
  __ CompareRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                 Heap::kPixelArrayMapRootIndex);
  __ j(not_equal, &slow);
  // Check that the value is a smi. If a conversion is needed call into the
  // runtime to convert and clamp.
  __ JumpIfNotSmi(rax, &slow);
  __ SmiToInteger32(rdi, rcx);
  __ cmpl(rdi, FieldOperand(rbx, PixelArray::kLengthOffset));
  __ j(above_equal, &slow);
  // No more bailouts to slow case on this path, so key not needed.
  __ SmiToInteger32(rcx, rax);
  {  // Clamp the value to [0..255].
    Label done;
    __ testl(rcx, Immediate(0xFFFFFF00));
    __ j(zero, &done);
    __ setcc(negative, rcx);  // 1 if negative, 0 if positive.
    __ decb(rcx);  // 0 if negative, 255 if positive.
    __ bind(&done);
  }
  __ movq(rbx, FieldOperand(rbx, PixelArray::kExternalPointerOffset));
  __ movb(Operand(rbx, rdi, times_1, 0), rcx);
  __ ret(0);

  // Extra capacity case: Check if there is extra capacity to
  // perform the store and update the length. Used for adding one
  // element to the array by writing to array[array.length].
  __ bind(&extra);
  // rax: value
  // rdx: receiver (a JSArray)
  // rbx: receiver's elements array (a FixedArray)
  // rcx: index (as a smi)
  // flags: smicompare (rdx.length(), rbx)
  __ j(not_equal, &slow);  // do not leave holes in the array
  __ SmiCompare(rcx, FieldOperand(rbx, FixedArray::kLengthOffset));
  __ j(above_equal, &slow);
  // Increment index to get new length.
  __ SmiAddConstant(rdi, rcx, Smi::FromInt(1));
  __ movq(FieldOperand(rdx, JSArray::kLengthOffset), rdi);
  __ jmp(&fast);

  // Array case: Get the length and the elements array from the JS
  // array. Check that the array is in fast mode; if it is the
  // length is always a smi.
  __ bind(&array);
  // rax: value
  // rdx: receiver (a JSArray)
  // rcx: index (as a smi)
  __ movq(rbx, FieldOperand(rdx, JSObject::kElementsOffset));
  __ CompareRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                 Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &slow);

  // Check the key against the length in the array, compute the
  // address to store into and fall through to fast case.
  __ SmiCompare(FieldOperand(rdx, JSArray::kLengthOffset), rcx);
  __ j(below_equal, &extra);

  // Fast case: Do the store.
  __ bind(&fast);
  // rax: value
  // rbx: receiver's elements array (a FixedArray)
  // rcx: index (as a smi)
  Label non_smi_value;
  __ JumpIfNotSmi(rax, &non_smi_value);
  SmiIndex index = masm->SmiToIndex(rcx, rcx, kPointerSizeLog2);
  __ movq(FieldOperand(rbx, index.reg, index.scale, FixedArray::kHeaderSize),
          rax);
  __ ret(0);
  __ bind(&non_smi_value);
  // Slow case that needs to retain rcx for use by RecordWrite.
  // Update write barrier for the elements array address.
  SmiIndex index2 = masm->SmiToIndex(kScratchRegister, rcx, kPointerSizeLog2);
  __ movq(FieldOperand(rbx, index2.reg, index2.scale, FixedArray::kHeaderSize),
          rax);
  __ movq(rdx, rax);
  __ RecordWriteNonSmi(rbx, 0, rdx, rcx);
  __ ret(0);
}


void KeyedStoreIC::GenerateExternalArray(MacroAssembler* masm,
                                         ExternalArrayType array_type) {
  // ----------- S t a t e -------------
  //  -- rax     : value
  //  -- rcx     : key
  //  -- rdx     : receiver
  //  -- rsp[0]  : return address
  // -----------------------------------
  Label slow, check_heap_number;

  // Check that the object isn't a smi.
  __ JumpIfSmi(rdx, &slow);
  // Get the map from the receiver.
  __ movq(rbx, FieldOperand(rdx, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.  We need
  // to do this because this generic stub does not perform map checks.
  __ testb(FieldOperand(rbx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &slow);
  // Check that the key is a smi.
  __ JumpIfNotSmi(rcx, &slow);

  // Check that the object is a JS object.
  __ CmpInstanceType(rbx, JS_OBJECT_TYPE);
  __ j(not_equal, &slow);

  // Check that the elements array is the appropriate type of
  // ExternalArray.
  // rax: value
  // rcx: key (a smi)
  // rdx: receiver (a JSObject)
  __ movq(rbx, FieldOperand(rdx, JSObject::kElementsOffset));
  __ CompareRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                 Heap::RootIndexForExternalArrayType(array_type));
  __ j(not_equal, &slow);

  // Check that the index is in range.
  __ SmiToInteger32(rdi, rcx);  // Untag the index.
  __ cmpl(rdi, FieldOperand(rbx, ExternalArray::kLengthOffset));
  // Unsigned comparison catches both negative and too-large values.
  __ j(above_equal, &slow);

  // Handle both smis and HeapNumbers in the fast path. Go to the
  // runtime for all other kinds of values.
  // rax: value
  // rcx: key (a smi)
  // rdx: receiver (a JSObject)
  // rbx: elements array
  // rdi: untagged key
  __ JumpIfNotSmi(rax, &check_heap_number);
  // No more branches to slow case on this path.  Key and receiver not needed.
  __ SmiToInteger32(rdx, rax);
  __ movq(rbx, FieldOperand(rbx, ExternalArray::kExternalPointerOffset));
  // rbx: base pointer of external storage
  switch (array_type) {
    case kExternalByteArray:
    case kExternalUnsignedByteArray:
      __ movb(Operand(rbx, rdi, times_1, 0), rdx);
      break;
    case kExternalShortArray:
    case kExternalUnsignedShortArray:
      __ movw(Operand(rbx, rdi, times_2, 0), rdx);
      break;
    case kExternalIntArray:
    case kExternalUnsignedIntArray:
      __ movl(Operand(rbx, rdi, times_4, 0), rdx);
      break;
    case kExternalFloatArray:
      // Need to perform int-to-float conversion.
      __ push(rdx);
      __ fild_s(Operand(rsp, 0));
      __ pop(rdx);
      __ fstp_s(Operand(rbx, rdi, times_4, 0));
      break;
    default:
      UNREACHABLE();
      break;
  }
  __ ret(0);

  __ bind(&check_heap_number);
  // rax: value
  // rcx: key (a smi)
  // rdx: receiver (a JSObject)
  // rbx: elements array
  // rdi: untagged key
  __ CmpObjectType(rax, HEAP_NUMBER_TYPE, kScratchRegister);
  __ j(not_equal, &slow);
  // No more branches to slow case on this path.

  // The WebGL specification leaves the behavior of storing NaN and
  // +/-Infinity into integer arrays basically undefined. For more
  // reproducible behavior, convert these to zero.
  __ fld_d(FieldOperand(rax, HeapNumber::kValueOffset));
  __ movq(rbx, FieldOperand(rbx, ExternalArray::kExternalPointerOffset));
  // rdi: untagged index
  // rbx: base pointer of external storage
  // top of FPU stack: value
  if (array_type == kExternalFloatArray) {
    __ fstp_s(Operand(rbx, rdi, times_4, 0));
    __ ret(0);
  } else {
    // Need to perform float-to-int conversion.
    // Test the top of the FP stack for NaN.
    Label is_nan;
    __ fucomi(0);
    __ j(parity_even, &is_nan);

    __ push(rdx);  // Make room on the stack.  Receiver is no longer needed.
    __ fistp_d(Operand(rsp, 0));
    __ pop(rdx);
    // rdx: value (converted to an untagged integer)
    // rdi: untagged index
    // rbx: base pointer of external storage
    switch (array_type) {
      case kExternalByteArray:
      case kExternalUnsignedByteArray:
        __ movb(Operand(rbx, rdi, times_1, 0), rdx);
        break;
      case kExternalShortArray:
      case kExternalUnsignedShortArray:
        __ movw(Operand(rbx, rdi, times_2, 0), rdx);
        break;
      case kExternalIntArray:
      case kExternalUnsignedIntArray: {
        // We also need to explicitly check for +/-Infinity. These are
        // converted to MIN_INT, but we need to be careful not to
        // confuse with legal uses of MIN_INT.  Since MIN_INT truncated
        // to 8 or 16 bits is zero, we only perform this test when storing
        // 32-bit ints.
        Label not_infinity;
        // This test would apparently detect both NaN and Infinity,
        // but we've already checked for NaN using the FPU hardware
        // above.
        __ movzxwq(rcx, FieldOperand(rax, HeapNumber::kValueOffset + 6));
        __ and_(rcx, Immediate(0x7FF0));
        __ cmpw(rcx, Immediate(0x7FF0));
        __ j(not_equal, &not_infinity);
        __ movq(rdx, Immediate(0));
        __ bind(&not_infinity);
        __ movl(Operand(rbx, rdi, times_4, 0), rdx);
        break;
      }
      default:
        UNREACHABLE();
        break;
    }
    __ ret(0);

    __ bind(&is_nan);
    // rdi: untagged index
    // rbx: base pointer of external storage
    __ ffree();
    __ fincstp();
    __ movq(rdx, Immediate(0));
    switch (array_type) {
      case kExternalByteArray:
      case kExternalUnsignedByteArray:
        __ movb(Operand(rbx, rdi, times_1, 0), rdx);
        break;
      case kExternalShortArray:
      case kExternalUnsignedShortArray:
        __ movw(Operand(rbx, rdi, times_2, 0), rdx);
        break;
      case kExternalIntArray:
      case kExternalUnsignedIntArray:
        __ movl(Operand(rbx, rdi, times_4, 0), rdx);
        break;
      default:
        UNREACHABLE();
        break;
    }
    __ ret(0);
  }

  // Slow case: call runtime.
  __ bind(&slow);
  GenerateRuntimeSetProperty(masm);
}


// Defined in ic.cc.
Object* CallIC_Miss(Arguments args);


static void GenerateCallMiss(MacroAssembler* masm, int argc, IC::UtilityId id) {
  // ----------- S t a t e -------------
  // rcx                      : function name
  // rsp[0]                   : return address
  // rsp[8]                   : argument argc
  // rsp[16]                  : argument argc - 1
  // ...
  // rsp[argc * 8]            : argument 1
  // rsp[(argc + 1) * 8]      : argument 0 = receiver
  // -----------------------------------
  // Get the receiver of the function from the stack; 1 ~ return address.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // Enter an internal frame.
  __ EnterInternalFrame();

  // Push the receiver and the name of the function.
  __ push(rdx);
  __ push(rcx);

  // Call the entry.
  CEntryStub stub(1);
  __ movq(rax, Immediate(2));
  __ movq(rbx, ExternalReference(IC_Utility(id)));
  __ CallStub(&stub);

  // Move result to rdi and exit the internal frame.
  __ movq(rdi, rax);
  __ LeaveInternalFrame();

  // Check if the receiver is a global object of some sort.
  Label invoke, global;
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));  // receiver
  __ JumpIfSmi(rdx, &invoke);
  __ CmpObjectType(rdx, JS_GLOBAL_OBJECT_TYPE, rcx);
  __ j(equal, &global);
  __ CmpInstanceType(rcx, JS_BUILTINS_OBJECT_TYPE);
  __ j(not_equal, &invoke);

  // Patch the receiver on the stack.
  __ bind(&global);
  __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalReceiverOffset));
  __ movq(Operand(rsp, (argc + 1) * kPointerSize), rdx);

  // Invoke the function.
  ParameterCount actual(argc);
  __ bind(&invoke);
  __ InvokeFunction(rdi, actual, JUMP_FUNCTION);
}


// The generated code does not accept smi keys.
// The generated code falls through if both probes miss.
static void GenerateMonomorphicCacheProbe(MacroAssembler* masm,
                                          int argc,
                                          Code::Kind kind) {
  // ----------- S t a t e -------------
  // rcx                      : function name
  // rdx                      : receiver
  // -----------------------------------
  Label number, non_number, non_string, boolean, probe, miss;

  // Probe the stub cache.
  Code::Flags flags =
      Code::ComputeFlags(kind, NOT_IN_LOOP, MONOMORPHIC, NORMAL, argc);
  StubCache::GenerateProbe(masm, flags, rdx, rcx, rbx, rax);

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
  StubCache::GenerateProbe(masm, flags, rdx, rcx, rbx, no_reg);

  __ bind(&miss);
}


static void GenerateNormalHelper(MacroAssembler* masm,
                                 int argc,
                                 bool is_global_object,
                                 Label* miss) {
  // ----------- S t a t e -------------
  // rcx                    : function name
  // rdx                    : receiver
  // rsp[0]                 : return address
  // rsp[8]                 : argument argc
  // rsp[16]                : argument argc - 1
  // ...
  // rsp[argc * 8]          : argument 1
  // rsp[(argc + 1) * 8]    : argument 0 = receiver
  // -----------------------------------
  // Search dictionary - put result in register rdx.
  GenerateDictionaryLoad(
     masm, miss, rax, rdx, rbx, rcx, rdi, rdi, CHECK_DICTIONARY);

  __ JumpIfSmi(rdi, miss);
  // Check that the value is a JavaScript function.
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rdx);
  __ j(not_equal, miss);

  // Patch the receiver with the global proxy if necessary.
  if (is_global_object) {
    __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalReceiverOffset));
    __ movq(Operand(rsp, (argc + 1) * kPointerSize), rdx);
  }

  // Invoke the function.
  ParameterCount actual(argc);
  __ InvokeFunction(rdi, actual, JUMP_FUNCTION);
}


// The generated code falls through if the call should be handled by runtime.
static void GenerateCallNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // rcx                    : function name
  // rsp[0]                 : return address
  // rsp[8]                 : argument argc
  // rsp[16]                : argument argc - 1
  // ...
  // rsp[argc * 8]          : argument 1
  // rsp[(argc + 1) * 8]    : argument 0 = receiver
  // -----------------------------------
  Label miss, global_object, non_global_object;

  // Get the receiver of the function from the stack.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(rdx, &miss);

  // Check that the receiver is a valid JS object.
  // Because there are so many map checks and type checks, do not
  // use CmpObjectType, but load map and type into registers.
  __ movq(rbx, FieldOperand(rdx, HeapObject::kMapOffset));
  __ movb(rax, FieldOperand(rbx, Map::kInstanceTypeOffset));
  __ cmpb(rax, Immediate(FIRST_JS_OBJECT_TYPE));
  __ j(below, &miss);

  // If this assert fails, we have to check upper bound too.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);

  // Check for access to global object.
  __ cmpb(rax, Immediate(JS_GLOBAL_OBJECT_TYPE));
  __ j(equal, &global_object);
  __ cmpb(rax, Immediate(JS_BUILTINS_OBJECT_TYPE));
  __ j(not_equal, &non_global_object);

  // Accessing global object: Load and invoke.
  __ bind(&global_object);
  // Check that the global object does not require access checks.
  __ movb(rbx, FieldOperand(rbx, Map::kBitFieldOffset));
  __ testb(rbx, Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_equal, &miss);
  GenerateNormalHelper(masm, argc, true, &miss);

  // Accessing non-global object: Check for access to global proxy.
  Label global_proxy, invoke;
  __ bind(&non_global_object);
  __ cmpb(rax, Immediate(JS_GLOBAL_PROXY_TYPE));
  __ j(equal, &global_proxy);
  // Check that the non-global, non-global-proxy object does not
  // require access checks.
  __ movb(rbx, FieldOperand(rbx, Map::kBitFieldOffset));
  __ testb(rbx, Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_equal, &miss);
  __ bind(&invoke);
  GenerateNormalHelper(masm, argc, false, &miss);

  // Global object proxy access: Check access rights.
  __ bind(&global_proxy);
  __ CheckAccessGlobalProxy(rdx, rax, &miss);
  __ jmp(&invoke);

  __ bind(&miss);
}


void CallIC::GenerateMiss(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // rcx                      : function name
  // rsp[0]                   : return address
  // rsp[8]                   : argument argc
  // rsp[16]                  : argument argc - 1
  // ...
  // rsp[argc * 8]            : argument 1
  // rsp[(argc + 1) * 8]      : argument 0 = receiver
  // -----------------------------------
  GenerateCallMiss(masm, argc, IC::kCallIC_Miss);
}


void CallIC::GenerateMegamorphic(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // rcx                      : function name
  // rsp[0]                   : return address
  // rsp[8]                   : argument argc
  // rsp[16]                  : argument argc - 1
  // ...
  // rsp[argc * 8]            : argument 1
  // rsp[(argc + 1) * 8]      : argument 0 = receiver
  // -----------------------------------

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));
  GenerateMonomorphicCacheProbe(masm, argc, Code::CALL_IC);
  GenerateMiss(masm, argc);
}


void CallIC::GenerateNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // rcx                      : function name
  // rsp[0]                   : return address
  // rsp[8]                   : argument argc
  // rsp[16]                  : argument argc - 1
  // ...
  // rsp[argc * 8]            : argument 1
  // rsp[(argc + 1) * 8]      : argument 0 = receiver
  // -----------------------------------

  GenerateCallNormal(masm, argc);
  GenerateMiss(masm, argc);
}


void KeyedCallIC::GenerateMiss(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // rcx                      : function name
  // rsp[0]                   : return address
  // rsp[8]                   : argument argc
  // rsp[16]                  : argument argc - 1
  // ...
  // rsp[argc * 8]            : argument 1
  // rsp[(argc + 1) * 8]      : argument 0 = receiver
  // -----------------------------------

  GenerateCallMiss(masm, argc, IC::kKeyedCallIC_Miss);
}


void KeyedCallIC::GenerateMegamorphic(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // rcx                      : function name
  // rsp[0]                   : return address
  // rsp[8]                   : argument argc
  // rsp[16]                  : argument argc - 1
  // ...
  // rsp[argc * 8]            : argument 1
  // rsp[(argc + 1) * 8]      : argument 0 = receiver
  // -----------------------------------

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));

  Label do_call, slow_call, slow_load, slow_reload_receiver;
  Label check_number_dictionary, check_string, lookup_monomorphic_cache;
  Label index_smi, index_string;

  // Check that the key is a smi.
  __ JumpIfNotSmi(rcx, &check_string);

  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from below
  // where a numeric string is converted to a smi.

  GenerateKeyedLoadReceiverCheck(masm, rdx, rax, &slow_call);

  GenerateFastArrayLoad(
      masm, rdx, rcx, rax, rbx, rdi, &check_number_dictionary, &slow_load);
  __ IncrementCounter(&Counters::keyed_call_generic_smi_fast, 1);

  __ bind(&do_call);
  // receiver in rdx is not used after this point.
  // rcx: key
  // rdi: function

  // Check that the value in edi is a JavaScript function.
  __ JumpIfSmi(rdi, &slow_call);
  __ CmpObjectType(rdi, JS_FUNCTION_TYPE, rax);
  __ j(not_equal, &slow_call);
  // Invoke the function.
  ParameterCount actual(argc);
  __ InvokeFunction(rdi, actual, JUMP_FUNCTION);

  __ bind(&check_number_dictionary);
  // eax: elements
  // ecx: smi key
  // Check whether the elements is a number dictionary.
  __ CompareRoot(FieldOperand(rax, HeapObject::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ SmiToInteger32(rbx, rcx);
  // ebx: untagged index
  GenerateNumberDictionaryLoad(masm, &slow_load, rax, rcx, rbx, r9, rdi, rdi);
  __ IncrementCounter(&Counters::keyed_call_generic_smi_dict, 1);
  __ jmp(&do_call);

  __ bind(&slow_load);
  // This branch is taken when calling KeyedCallIC_Miss is neither required
  // nor beneficial.
  __ IncrementCounter(&Counters::keyed_call_generic_slow_load, 1);
  __ EnterInternalFrame();
  __ push(rcx);  // save the key
  __ push(rdx);  // pass the receiver
  __ push(rcx);  // pass the key
  __ CallRuntime(Runtime::kKeyedGetProperty, 2);
  __ pop(rcx);  // restore the key
  __ LeaveInternalFrame();
  __ movq(rdi, rax);
  __ jmp(&do_call);

  __ bind(&check_string);
  GenerateKeyStringCheck(masm, rcx, rax, rbx, &index_string, &slow_call);

  // The key is known to be a symbol.
  // If the receiver is a regular JS object with slow properties then do
  // a quick inline probe of the receiver's dictionary.
  // Otherwise do the monomorphic cache probe.
  GenerateKeyedLoadReceiverCheck(masm, rdx, rax, &lookup_monomorphic_cache);

  __ movq(rbx, FieldOperand(rdx, JSObject::kPropertiesOffset));
  __ CompareRoot(FieldOperand(rbx, HeapObject::kMapOffset),
                 Heap::kHashTableMapRootIndex);
  __ j(not_equal, &lookup_monomorphic_cache);

  GenerateDictionaryLoad(
      masm, &slow_load, rbx, rdx, rax, rcx, rdi, rdi, DICTIONARY_CHECK_DONE);
  __ IncrementCounter(&Counters::keyed_call_generic_lookup_dict, 1);
  __ jmp(&do_call);

  __ bind(&lookup_monomorphic_cache);
  __ IncrementCounter(&Counters::keyed_call_generic_lookup_cache, 1);
  GenerateMonomorphicCacheProbe(masm, argc, Code::KEYED_CALL_IC);
  // Fall through on miss.

  __ bind(&slow_call);
  // This branch is taken if:
  // - the receiver requires boxing or access check,
  // - the key is neither smi nor symbol,
  // - the value loaded is not a function,
  // - there is hope that the runtime will create a monomorphic call stub
  //   that will get fetched next time.
  __ IncrementCounter(&Counters::keyed_call_generic_slow, 1);
  GenerateMiss(masm, argc);

  __ bind(&index_string);
  GenerateIndexFromHash(masm, rcx, rbx);
  // Now jump to the place where smi keys are handled.
  __ jmp(&index_smi);
}


void KeyedCallIC::GenerateNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // rcx                      : function name
  // rsp[0]                   : return address
  // rsp[8]                   : argument argc
  // rsp[16]                  : argument argc - 1
  // ...
  // rsp[argc * 8]            : argument 1
  // rsp[(argc + 1) * 8]      : argument 0 = receiver
  // -----------------------------------

  GenerateCallNormal(masm, argc);
  GenerateMiss(masm, argc);
}


// The offset from the inlined patch site to the start of the
// inlined load instruction.
const int LoadIC::kOffsetToLoadInstruction = 20;


void LoadIC::ClearInlinedVersion(Address address) {
  // Reset the map check of the inlined inobject property load (if
  // present) to guarantee failure by holding an invalid map (the null
  // value).  The offset can be patched to anything.
  PatchInlinedLoad(address, Heap::null_value(), kMaxInt);
}


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------

  __ pop(rbx);
  __ push(rax);  // receiver
  __ push(rcx);  // name
  __ push(rbx);  // return address

  // Perform tail call to the entry.
  ExternalReference ref = ExternalReference(IC_Utility(kLoadIC_Miss));
  __ TailCallExternalReference(ref, 2, 1);
}


void LoadIC::GenerateArrayLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  StubCompiler::GenerateLoadArrayLength(masm, rax, rdx, &miss);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateFunctionPrototype(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  StubCompiler::GenerateLoadFunctionPrototype(masm, rax, rdx, rbx, &miss);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------

  // Probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(Code::LOAD_IC,
                                         NOT_IN_LOOP,
                                         MONOMORPHIC);
  StubCache::GenerateProbe(masm, flags, rax, rcx, rbx, rdx);

  // Cache miss: Jump to runtime.
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateNormal(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss, probe, global;

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(rax, &miss);

  // Check that the receiver is a valid JS object.
  __ CmpObjectType(rax, FIRST_JS_OBJECT_TYPE, rbx);
  __ j(below, &miss);

  // If this assert fails, we have to check upper bound too.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);

  // Check for access to global object (unlikely).
  __ CmpInstanceType(rbx, JS_GLOBAL_PROXY_TYPE);
  __ j(equal, &global);

  // Check for non-global object that requires access check.
  __ testl(FieldOperand(rbx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &miss);

  // Search the dictionary placing the result in rax.
  __ bind(&probe);
  GenerateDictionaryLoad(masm, &miss, rdx, rax, rbx,
                         rcx, rdi, rax, CHECK_DICTIONARY);
  __ ret(0);

  // Global object access: Check access rights.
  __ bind(&global);
  __ CheckAccessGlobalProxy(rax, rdx, &miss);
  __ jmp(&probe);

  // Cache miss: Jump to runtime.
  __ bind(&miss);
  GenerateMiss(masm);
}


void LoadIC::GenerateStringLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : receiver
  //  -- rcx    : name
  //  -- rsp[0] : return address
  // -----------------------------------
  Label miss;

  StubCompiler::GenerateLoadStringLength(masm, rax, rdx, rbx, &miss);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


bool LoadIC::PatchInlinedLoad(Address address, Object* map, int offset) {
  // The address of the instruction following the call.
  Address test_instruction_address =
      address + Assembler::kCallTargetAddressOffset;
  // If the instruction following the call is not a test eax, nothing
  // was inlined.
  if (*test_instruction_address != kTestEaxByte) return false;

  Address delta_address = test_instruction_address + 1;
  // The delta to the start of the map check instruction.
  int delta = *reinterpret_cast<int*>(delta_address);

  // The map address is the last 8 bytes of the 10-byte
  // immediate move instruction, so we add 2 to get the
  // offset to the last 8 bytes.
  Address map_address = test_instruction_address + delta + 2;
  *(reinterpret_cast<Object**>(map_address)) = map;

  // The offset is in the 32-bit displacement of a seven byte
  // memory-to-register move instruction (REX.W 0x88 ModR/M disp32),
  // so we add 3 to get the offset of the displacement.
  Address offset_address =
      test_instruction_address + delta + kOffsetToLoadInstruction + 3;
  *reinterpret_cast<int*>(offset_address) = offset - kHeapObjectTag;
  return true;
}


void StoreIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------

  __ pop(rbx);
  __ push(rdx);  // receiver
  __ push(rcx);  // name
  __ push(rax);  // value
  __ push(rbx);  // return address

  // Perform tail call to the entry.
  ExternalReference ref = ExternalReference(IC_Utility(kStoreIC_Miss));
  __ TailCallExternalReference(ref, 3, 1);
}


void StoreIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------

  // Get the receiver from the stack and probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(Code::STORE_IC,
                                         NOT_IN_LOOP,
                                         MONOMORPHIC);
  StubCache::GenerateProbe(masm, flags, rdx, rcx, rbx, no_reg);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
}


void StoreIC::GenerateArrayLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  //
  // This accepts as a receiver anything JSObject::SetElementsLength accepts
  // (currently anything except for external and pixel arrays which means
  // anything with elements of FixedArray type.), but currently is restricted
  // to JSArray.
  // Value must be a number, but only smis are accepted as the most common case.

  Label miss;

  Register receiver = rdx;
  Register value = rax;
  Register scratch = rbx;

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Check that the object is a JS array.
  __ CmpObjectType(receiver, JS_ARRAY_TYPE, scratch);
  __ j(not_equal, &miss);

  // Check that elements are FixedArray.
  __ movq(scratch, FieldOperand(receiver, JSArray::kElementsOffset));
  __ CmpObjectType(scratch, FIXED_ARRAY_TYPE, scratch);
  __ j(not_equal, &miss);

  // Check that value is a smi.
  __ JumpIfNotSmi(value, &miss);

  // Prepare tail call to StoreIC_ArrayLength.
  __ pop(scratch);
  __ push(receiver);
  __ push(value);
  __ push(scratch);  // return address

  ExternalReference ref = ExternalReference(IC_Utility(kStoreIC_ArrayLength));
  __ TailCallExternalReference(ref, 2, 1);

  __ bind(&miss);

  GenerateMiss(masm);
}


#undef __


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
