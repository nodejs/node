// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
                                   Register receiver,
                                   Register name,
                                   Register r0,
                                   Register r1,
                                   Register r2,
                                   DictionaryCheck check_dictionary) {
  // Register use:
  //
  // name - holds the name of the property and is unchanged.
  // receiver - holds the receiver and is unchanged.
  // Scratch registers:
  // r0   - used to hold the property dictionary.
  //
  // r1   - used for the index into the property dictionary
  //      - holds the result on exit.
  //
  // r2   - used to hold the capacity of the property dictionary.

  Label done;

  // Check for the absence of an interceptor.
  // Load the map into r0.
  __ mov(r0, FieldOperand(receiver, JSObject::kMapOffset));

  // Bail out if the receiver has a named interceptor.
  __ test(FieldOperand(r0, Map::kBitFieldOffset),
          Immediate(1 << Map::kHasNamedInterceptor));
  __ j(not_zero, miss_label, not_taken);

  // Bail out if we have a JS global proxy object.
  __ movzx_b(r0, FieldOperand(r0, Map::kInstanceTypeOffset));
  __ cmp(r0, JS_GLOBAL_PROXY_TYPE);
  __ j(equal, miss_label, not_taken);

  // Possible work-around for http://crbug.com/16276.
  __ cmp(r0, JS_GLOBAL_OBJECT_TYPE);
  __ j(equal, miss_label, not_taken);
  __ cmp(r0, JS_BUILTINS_OBJECT_TYPE);
  __ j(equal, miss_label, not_taken);

  // Load properties array.
  __ mov(r0, FieldOperand(receiver, JSObject::kPropertiesOffset));

  // Check that the properties array is a dictionary.
  if (check_dictionary == CHECK_DICTIONARY) {
    __ cmp(FieldOperand(r0, HeapObject::kMapOffset),
           Immediate(Factory::hash_table_map()));
    __ j(not_equal, miss_label);
  }

  // Compute the capacity mask.
  const int kCapacityOffset =
      StringDictionary::kHeaderSize +
      StringDictionary::kCapacityIndex * kPointerSize;
  __ mov(r2, FieldOperand(r0, kCapacityOffset));
  __ shr(r2, kSmiTagSize);  // convert smi to int
  __ dec(r2);

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  static const int kProbes = 4;
  const int kElementsStartOffset =
      StringDictionary::kHeaderSize +
      StringDictionary::kElementsStartIndex * kPointerSize;
  for (int i = 0; i < kProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ mov(r1, FieldOperand(name, String::kHashFieldOffset));
    __ shr(r1, String::kHashShift);
    if (i > 0) {
      __ add(Operand(r1), Immediate(StringDictionary::GetProbeOffset(i)));
    }
    __ and_(r1, Operand(r2));

    // Scale the index by multiplying by the entry size.
    ASSERT(StringDictionary::kEntrySize == 3);
    __ lea(r1, Operand(r1, r1, times_2, 0));  // r1 = r1 * 3

    // Check if the key is identical to the name.
    __ cmp(name,
           Operand(r0, r1, times_4, kElementsStartOffset - kHeapObjectTag));
    if (i != kProbes - 1) {
      __ j(equal, &done, taken);
    } else {
      __ j(not_equal, miss_label, not_taken);
    }
  }

  // Check that the value is a normal property.
  __ bind(&done);
  const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  __ test(Operand(r0, r1, times_4, kDetailsOffset - kHeapObjectTag),
          Immediate(PropertyDetails::TypeField::mask() << kSmiTagSize));
  __ j(not_zero, miss_label, not_taken);

  // Get the value at the masked, scaled index.
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ mov(r1, Operand(r0, r1, times_4, kValueOffset - kHeapObjectTag));
}


static void GenerateNumberDictionaryLoad(MacroAssembler* masm,
                                         Label* miss,
                                         Register elements,
                                         Register key,
                                         Register r0,
                                         Register r1,
                                         Register r2) {
  // Register use:
  //
  // elements - holds the slow-case elements of the receiver and is unchanged.
  //
  // key      - holds the smi key on entry and is unchanged if a branch is
  //            performed to the miss label. If the load succeeds and we
  //            fall through, key holds the result on exit.
  //
  // Scratch registers:
  //
  // r0 - holds the untagged key on entry and holds the hash once computed.
  //
  // r1 - used to hold the capacity mask of the dictionary
  //
  // r2 - used for the index into the dictionary.
  Label done;

  // Compute the hash code from the untagged key.  This must be kept in sync
  // with ComputeIntegerHash in utils.h.
  //
  // hash = ~hash + (hash << 15);
  __ mov(r1, r0);
  __ not_(r0);
  __ shl(r1, 15);
  __ add(r0, Operand(r1));
  // hash = hash ^ (hash >> 12);
  __ mov(r1, r0);
  __ shr(r1, 12);
  __ xor_(r0, Operand(r1));
  // hash = hash + (hash << 2);
  __ lea(r0, Operand(r0, r0, times_4, 0));
  // hash = hash ^ (hash >> 4);
  __ mov(r1, r0);
  __ shr(r1, 4);
  __ xor_(r0, Operand(r1));
  // hash = hash * 2057;
  __ imul(r0, r0, 2057);
  // hash = hash ^ (hash >> 16);
  __ mov(r1, r0);
  __ shr(r1, 16);
  __ xor_(r0, Operand(r1));

  // Compute capacity mask.
  __ mov(r1, FieldOperand(elements, NumberDictionary::kCapacityOffset));
  __ shr(r1, kSmiTagSize);  // convert smi to int
  __ dec(r1);

  // Generate an unrolled loop that performs a few probes before giving up.
  const int kProbes = 4;
  for (int i = 0; i < kProbes; i++) {
    // Use r2 for index calculations and keep the hash intact in r0.
    __ mov(r2, r0);
    // Compute the masked index: (hash + i + i * i) & mask.
    if (i > 0) {
      __ add(Operand(r2), Immediate(NumberDictionary::GetProbeOffset(i)));
    }
    __ and_(r2, Operand(r1));

    // Scale the index by multiplying by the entry size.
    ASSERT(NumberDictionary::kEntrySize == 3);
    __ lea(r2, Operand(r2, r2, times_2, 0));  // r2 = r2 * 3

    // Check if the key matches.
    __ cmp(key, FieldOperand(elements,
                             r2,
                             times_pointer_size,
                             NumberDictionary::kElementsStartOffset));
    if (i != (kProbes - 1)) {
      __ j(equal, &done, taken);
    } else {
      __ j(not_equal, miss, not_taken);
    }
  }

  __ bind(&done);
  // Check that the value is a normal propety.
  const int kDetailsOffset =
      NumberDictionary::kElementsStartOffset + 2 * kPointerSize;
  ASSERT_EQ(NORMAL, 0);
  __ test(FieldOperand(elements, r2, times_pointer_size, kDetailsOffset),
          Immediate(PropertyDetails::TypeField::mask() << kSmiTagSize));
  __ j(not_zero, miss);

  // Get the value at the masked, scaled index.
  const int kValueOffset =
      NumberDictionary::kElementsStartOffset + kPointerSize;
  __ mov(key, FieldOperand(elements, r2, times_pointer_size, kValueOffset));
}


// The offset from the inlined patch site to the start of the
// inlined load instruction.  It is 7 bytes (test eax, imm) plus
// 6 bytes (jne slow_label).
const int LoadIC::kOffsetToLoadInstruction = 13;


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


void LoadIC::GenerateStringLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;

  StubCompiler::GenerateLoadStringLength(masm, eax, edx, ebx, &miss);
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


void KeyedLoadIC::GenerateGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label slow, check_string, index_int, index_string;
  Label check_pixel_array, probe_dictionary;
  Label check_number_dictionary;

  // Check that the object isn't a smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &slow, not_taken);

  // Get the map of the receiver.
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));

  // Check bit field.
  __ movzx_b(ebx, FieldOperand(ecx, Map::kBitFieldOffset));
  __ test(ebx, Immediate(kSlowCaseBitFieldMask));
  __ j(not_zero, &slow, not_taken);
  // Check that the object is some kind of JS object EXCEPT JS Value type.
  // In the case that the object is a value-wrapper object,
  // we enter the runtime system to make sure that indexing
  // into string objects work as intended.
  ASSERT(JS_OBJECT_TYPE > JS_VALUE_TYPE);
  __ CmpInstanceType(ecx, JS_OBJECT_TYPE);
  __ j(below, &slow, not_taken);
  // Check that the key is a smi.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(not_zero, &check_string, not_taken);
  __ mov(ebx, eax);
  __ SmiUntag(ebx);
  // Get the elements array of the object.
  __ bind(&index_int);
  __ mov(ecx, FieldOperand(edx, JSObject::kElementsOffset));
  // Check that the object is in fast mode (not dictionary).
  __ CheckMap(ecx, Factory::fixed_array_map(), &check_pixel_array, true);
  // Check that the key (index) is within bounds.
  __ cmp(ebx, FieldOperand(ecx, FixedArray::kLengthOffset));
  __ j(above_equal, &slow);
  // Fast case: Do the load.
  __ mov(ecx, FieldOperand(ecx, ebx, times_4, FixedArray::kHeaderSize));
  __ cmp(Operand(ecx), Immediate(Factory::the_hole_value()));
  // In case the loaded value is the_hole we have to consult GetProperty
  // to ensure the prototype chain is searched.
  __ j(equal, &slow);
  __ mov(eax, ecx);
  __ IncrementCounter(&Counters::keyed_load_generic_smi, 1);
  __ ret(0);

  __ bind(&check_pixel_array);
  // Check whether the elements is a pixel array.
  // edx: receiver
  // ebx: untagged index
  // eax: key
  // ecx: elements
  __ CheckMap(ecx, Factory::pixel_array_map(), &check_number_dictionary, true);
  __ cmp(ebx, FieldOperand(ecx, PixelArray::kLengthOffset));
  __ j(above_equal, &slow);
  __ mov(eax, FieldOperand(ecx, PixelArray::kExternalPointerOffset));
  __ movzx_b(eax, Operand(eax, ebx, times_1, 0));
  __ SmiTag(eax);
  __ ret(0);

  __ bind(&check_number_dictionary);
  // Check whether the elements is a number dictionary.
  // edx: receiver
  // ebx: untagged index
  // eax: key
  // ecx: elements
  __ CheckMap(ecx, Factory::hash_table_map(), &slow, true);
  Label slow_pop_receiver;
  // Push receiver on the stack to free up a register for the dictionary
  // probing.
  __ push(edx);
  GenerateNumberDictionaryLoad(masm,
                               &slow_pop_receiver,
                               ecx,
                               eax,
                               ebx,
                               edx,
                               edi);
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
  __ IncrementCounter(&Counters::keyed_load_generic_slow, 1);
  GenerateRuntimeGetProperty(masm);

  __ bind(&check_string);
  // The key is not a smi.
  // Is it a string?
  // edx: receiver
  // eax: key
  __ CmpObjectType(eax, FIRST_NONSTRING_TYPE, ecx);
  __ j(above_equal, &slow);
  // Is the string an array index, with cached numeric value?
  __ mov(ebx, FieldOperand(eax, String::kHashFieldOffset));
  __ test(ebx, Immediate(String::kIsArrayIndexMask));
  __ j(not_zero, &index_string, not_taken);

  // Is the string a symbol?
  __ movzx_b(ebx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  ASSERT(kSymbolTag != 0);
  __ test(ebx, Immediate(kIsSymbolMask));
  __ j(zero, &slow, not_taken);

  // If the receiver is a fast-case object, check the keyed lookup
  // cache. Otherwise probe the dictionary.
  __ mov(ebx, FieldOperand(edx, JSObject::kPropertiesOffset));
  __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
         Immediate(Factory::hash_table_map()));
  __ j(equal, &probe_dictionary);

  // Load the map of the receiver, compute the keyed lookup cache hash
  // based on 32 bits of the map pointer and the string hash.
  __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));
  __ mov(ecx, ebx);
  __ shr(ecx, KeyedLookupCache::kMapHashShift);
  __ mov(edi, FieldOperand(eax, String::kHashFieldOffset));
  __ shr(edi, String::kHashShift);
  __ xor_(ecx, Operand(edi));
  __ and_(ecx, KeyedLookupCache::kCapacityMask);

  // Load the key (consisting of map and symbol) from the cache and
  // check for match.
  ExternalReference cache_keys
      = ExternalReference::keyed_lookup_cache_keys();
  __ mov(edi, ecx);
  __ shl(edi, kPointerSizeLog2 + 1);
  __ cmp(ebx, Operand::StaticArray(edi, times_1, cache_keys));
  __ j(not_equal, &slow);
  __ add(Operand(edi), Immediate(kPointerSize));
  __ cmp(eax, Operand::StaticArray(edi, times_1, cache_keys));
  __ j(not_equal, &slow);

  // Get field offset and check that it is an in-object property.
  // edx     : receiver
  // ebx     : receiver's map
  // eax     : key
  // ecx     : lookup cache index
  ExternalReference cache_field_offsets
      = ExternalReference::keyed_lookup_cache_field_offsets();
  __ mov(edi,
         Operand::StaticArray(ecx, times_pointer_size, cache_field_offsets));
  __ movzx_b(ecx, FieldOperand(ebx, Map::kInObjectPropertiesOffset));
  __ cmp(edi, Operand(ecx));
  __ j(above_equal, &slow);

  // Load in-object property.
  __ sub(edi, Operand(ecx));
  __ movzx_b(ecx, FieldOperand(ebx, Map::kInstanceSizeOffset));
  __ add(ecx, Operand(edi));
  __ mov(eax, FieldOperand(edx, ecx, times_pointer_size, 0));
  __ ret(0);

  // Do a quick inline probe of the receiver's dictionary, if it
  // exists.
  __ bind(&probe_dictionary);
  GenerateDictionaryLoad(masm,
                         &slow,
                         edx,
                         eax,
                         ebx,
                         ecx,
                         edi,
                         DICTIONARY_CHECK_DONE);
  __ mov(eax, ecx);
  __ IncrementCounter(&Counters::keyed_load_generic_symbol, 1);
  __ ret(0);

  // If the hash field contains an array index pick it out. The assert checks
  // that the constants for the maximum number of digits for an array index
  // cached in the hash field and the number of bits reserved for it does not
  // conflict.
  ASSERT(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << String::kArrayIndexValueBits));
  __ bind(&index_string);
  __ and_(ebx, String::kArrayIndexHashMask);
  __ shr(ebx, String::kHashShift);
  __ jmp(&index_int);
}


void KeyedLoadIC::GenerateString(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : key (index)
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss;
  Label not_positive_smi;
  Label slow_char_code;
  Label got_char_code;

  Register receiver = edx;
  Register index = eax;
  Register code = ebx;
  Register scratch = ecx;

  StringHelper::GenerateFastCharCodeAt(masm,
                                       receiver,
                                       index,
                                       scratch,
                                       code,
                                       &miss,  // When not a string.
                                       &not_positive_smi,
                                       &slow_char_code);
  // If we didn't bail out, code register contains smi tagged char
  // code.
  __ bind(&got_char_code);
  StringHelper::GenerateCharFromCode(masm, code, eax, JUMP_FUNCTION);
#ifdef DEBUG
  __ Abort("Unexpected fall-through from char from code tail call");
#endif

  // Check if key is a smi or a heap number.
  __ bind(&not_positive_smi);
  ASSERT(kSmiTag == 0);
  __ test(index, Immediate(kSmiTagMask));
  __ j(zero, &slow_char_code);
  __ mov(ecx, FieldOperand(eax, HeapObject::kMapOffset));
  __ cmp(ecx, Factory::heap_number_map());
  __ j(not_equal, &miss);

  // Push receiver and key on the stack (now that we know they are a
  // string and a number), and call runtime.
  __ bind(&slow_char_code);
  __ EnterInternalFrame();
  __ push(receiver);
  __ push(index);
  __ CallRuntime(Runtime::kStringCharCodeAt, 2);
  ASSERT(!code.is(eax));
  __ mov(code, eax);
  __ LeaveInternalFrame();

  // Check if the runtime call returned NaN char code. If yes, return
  // undefined. Otherwise, we can continue.
  if (FLAG_debug_code) {
    ASSERT(kSmiTag == 0);
    __ test(code, Immediate(kSmiTagMask));
    __ j(zero, &got_char_code);
    __ mov(scratch, FieldOperand(code, HeapObject::kMapOffset));
    __ cmp(scratch, Factory::heap_number_map());
    __ Assert(equal, "StringCharCodeAt must return smi or heap number");
  }
  __ cmp(code, Factory::nan_value());
  __ j(not_equal, &got_char_code);
  __ Set(eax, Immediate(Factory::undefined_value()));
  __ ret(0);

  __ bind(&miss);
  GenerateMiss(masm);
}


void KeyedLoadIC::GenerateExternalArray(MacroAssembler* masm,
                                        ExternalArrayType array_type) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label slow, failed_allocation;

  // Check that the object isn't a smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &slow, not_taken);

  // Check that the key is a smi.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(not_zero, &slow, not_taken);

  // Get the map of the receiver.
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.  We need
  // to check this explicitly since this generic stub does not perform
  // map checks.
  __ movzx_b(ebx, FieldOperand(ecx, Map::kBitFieldOffset));
  __ test(ebx, Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &slow, not_taken);

  __ CmpInstanceType(ecx, JS_OBJECT_TYPE);
  __ j(not_equal, &slow, not_taken);

  // Check that the elements array is the appropriate type of
  // ExternalArray.
  __ mov(ebx, FieldOperand(edx, JSObject::kElementsOffset));
  Handle<Map> map(Heap::MapForExternalArrayType(array_type));
  __ cmp(FieldOperand(ebx, HeapObject::kMapOffset),
         Immediate(map));
  __ j(not_equal, &slow, not_taken);

  // eax: key, known to be a smi.
  // edx: receiver, known to be a JSObject.
  // ebx: elements object, known to be an external array.
  // Check that the index is in range.
  __ mov(ecx, eax);
  __ SmiUntag(ecx);  // Untag the index.
  __ cmp(ecx, FieldOperand(ebx, ExternalArray::kLengthOffset));
  // Unsigned comparison catches both negative and too-large values.
  __ j(above_equal, &slow);

  __ mov(ebx, FieldOperand(ebx, ExternalArray::kExternalPointerOffset));
  // ebx: base pointer of external storage
  switch (array_type) {
    case kExternalByteArray:
      __ movsx_b(ecx, Operand(ebx, ecx, times_1, 0));
      break;
    case kExternalUnsignedByteArray:
      __ movzx_b(ecx, Operand(ebx, ecx, times_1, 0));
      break;
    case kExternalShortArray:
      __ movsx_w(ecx, Operand(ebx, ecx, times_2, 0));
      break;
    case kExternalUnsignedShortArray:
      __ movzx_w(ecx, Operand(ebx, ecx, times_2, 0));
      break;
    case kExternalIntArray:
    case kExternalUnsignedIntArray:
      __ mov(ecx, Operand(ebx, ecx, times_4, 0));
      break;
    case kExternalFloatArray:
      __ fld_s(Operand(ebx, ecx, times_4, 0));
      break;
    default:
      UNREACHABLE();
      break;
  }

  // For integer array types:
  // ecx: value
  // For floating-point array type:
  // FP(0): value

  if (array_type == kExternalIntArray ||
      array_type == kExternalUnsignedIntArray) {
    // For the Int and UnsignedInt array types, we need to see whether
    // the value can be represented in a Smi. If not, we need to convert
    // it to a HeapNumber.
    Label box_int;
    if (array_type == kExternalIntArray) {
      __ cmp(ecx, 0xC0000000);
      __ j(sign, &box_int);
    } else {
      ASSERT_EQ(array_type, kExternalUnsignedIntArray);
      // The test is different for unsigned int values. Since we need
      // the value to be in the range of a positive smi, we can't
      // handle either of the top two bits being set in the value.
      __ test(ecx, Immediate(0xC0000000));
      __ j(not_zero, &box_int);
    }

    __ mov(eax, ecx);
    __ SmiTag(eax);
    __ ret(0);

    __ bind(&box_int);

    // Allocate a HeapNumber for the int and perform int-to-double
    // conversion.
    if (array_type == kExternalIntArray) {
      __ push(ecx);
      __ fild_s(Operand(esp, 0));
      __ pop(ecx);
    } else {
      ASSERT(array_type == kExternalUnsignedIntArray);
      // Need to zero-extend the value.
      // There's no fild variant for unsigned values, so zero-extend
      // to a 64-bit int manually.
      __ push(Immediate(0));
      __ push(ecx);
      __ fild_d(Operand(esp, 0));
      __ pop(ecx);
      __ pop(ecx);
    }
    // FP(0): value
    __ AllocateHeapNumber(ecx, ebx, edi, &failed_allocation);
    // Set the value.
    __ mov(eax, ecx);
    __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
    __ ret(0);
  } else if (array_type == kExternalFloatArray) {
    // For the floating-point array type, we need to always allocate a
    // HeapNumber.
    __ AllocateHeapNumber(ecx, ebx, edi, &failed_allocation);
    // Set the value.
    __ mov(eax, ecx);
    __ fstp_d(FieldOperand(eax, HeapNumber::kValueOffset));
    __ ret(0);
  } else {
    __ mov(eax, ecx);
    __ SmiTag(eax);
    __ ret(0);
  }

  // If we fail allocation of the HeapNumber, we still have a value on
  // top of the FPU stack. Remove it.
  __ bind(&failed_allocation);
  __ ffree();
  __ fincstp();
  // Fall through to slow case.

  // Slow case: Load key and receiver from stack and jump to runtime.
  __ bind(&slow);
  __ IncrementCounter(&Counters::keyed_load_external_array_slow, 1);
  GenerateRuntimeGetProperty(masm);
}


void KeyedLoadIC::GenerateIndexedInterceptor(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label slow;

  // Check that the receiver isn't a smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &slow, not_taken);

  // Check that the key is a smi.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(not_zero, &slow, not_taken);

  // Get the map of the receiver.
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));

  // Check that it has indexed interceptor and access checks
  // are not enabled for this object.
  __ movzx_b(ecx, FieldOperand(ecx, Map::kBitFieldOffset));
  __ and_(Operand(ecx), Immediate(kSlowCaseBitFieldMask));
  __ cmp(Operand(ecx), Immediate(1 << Map::kHasIndexedInterceptor));
  __ j(not_zero, &slow, not_taken);

  // Everything is fine, call runtime.
  __ pop(ecx);
  __ push(edx);  // receiver
  __ push(eax);  // key
  __ push(ecx);  // return address

  // Perform tail call to the entry.
  ExternalReference ref = ExternalReference(
      IC_Utility(kKeyedLoadPropertyWithInterceptor));
  __ TailCallExternalReference(ref, 2, 1);

  __ bind(&slow);
  GenerateMiss(masm);
}


void KeyedStoreIC::GenerateGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label slow, fast, array, extra, check_pixel_array;

  // Check that the object isn't a smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &slow, not_taken);
  // Get the map from the receiver.
  __ mov(edi, FieldOperand(edx, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.  We need
  // to do this because this generic stub does not perform map checks.
  __ movzx_b(ebx, FieldOperand(edi, Map::kBitFieldOffset));
  __ test(ebx, Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &slow, not_taken);
  // Check that the key is a smi.
  __ test(ecx, Immediate(kSmiTagMask));
  __ j(not_zero, &slow, not_taken);
  __ CmpInstanceType(edi, JS_ARRAY_TYPE);
  __ j(equal, &array);
  // Check that the object is some kind of JS object.
  __ CmpInstanceType(edi, FIRST_JS_OBJECT_TYPE);
  __ j(below, &slow, not_taken);

  // Object case: Check key against length in the elements array.
  // eax: value
  // edx: JSObject
  // ecx: key (a smi)
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
  // Check that the object is in fast mode (not dictionary).
  __ CheckMap(edi, Factory::fixed_array_map(), &check_pixel_array, true);
  __ mov(ebx, Operand(ecx));
  __ SmiUntag(ebx);
  __ cmp(ebx, FieldOperand(edi, Array::kLengthOffset));
  __ j(below, &fast, taken);

  // Slow case: call runtime.
  __ bind(&slow);
  GenerateRuntimeSetProperty(masm);

  // Check whether the elements is a pixel array.
  __ bind(&check_pixel_array);
  // eax: value
  // ecx: key
  // edx: receiver
  // edi: elements array
  __ CheckMap(edi, Factory::pixel_array_map(), &slow, true);
  // Check that the value is a smi. If a conversion is needed call into the
  // runtime to convert and clamp.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(not_zero, &slow);
  __ mov(ebx, ecx);
  __ SmiUntag(ebx);
  __ cmp(ebx, FieldOperand(edi, PixelArray::kLengthOffset));
  __ j(above_equal, &slow);
  __ mov(ecx, eax);  // Save the value. Key is not longer needed.
  __ SmiUntag(ecx);
  {  // Clamp the value to [0..255].
    Label done;
    __ test(ecx, Immediate(0xFFFFFF00));
    __ j(zero, &done);
    __ setcc(negative, ecx);  // 1 if negative, 0 if positive.
    __ dec_b(ecx);  // 0 if negative, 255 if positive.
    __ bind(&done);
  }
  __ mov(edi, FieldOperand(edi, PixelArray::kExternalPointerOffset));
  __ mov_b(Operand(edi, ebx, times_1, 0), ecx);
  __ ret(0);  // Return value in eax.

  // Extra capacity case: Check if there is extra capacity to
  // perform the store and update the length. Used for adding one
  // element to the array by writing to array[array.length].
  __ bind(&extra);
  // eax: value
  // edx: receiver, a JSArray
  // ecx: key, a smi.
  // edi: receiver->elements, a FixedArray
  // flags: compare (ecx, edx.length())
  __ j(not_equal, &slow, not_taken);  // do not leave holes in the array
  __ mov(ebx, ecx);
  __ SmiUntag(ebx);  // untag
  __ cmp(ebx, FieldOperand(edi, Array::kLengthOffset));
  __ j(above_equal, &slow, not_taken);
  // Add 1 to receiver->length, and go to fast array write.
  __ add(FieldOperand(edx, JSArray::kLengthOffset),
         Immediate(1 << kSmiTagSize));
  __ jmp(&fast);

  // Array case: Get the length and the elements array from the JS
  // array. Check that the array is in fast mode; if it is the
  // length is always a smi.
  __ bind(&array);
  // eax: value
  // edx: receiver, a JSArray
  // ecx: key, a smi.
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
  __ CheckMap(edi, Factory::fixed_array_map(), &check_pixel_array, true);

  // Check the key against the length in the array, compute the
  // address to store into and fall through to fast case.
  __ cmp(ecx, FieldOperand(edx, JSArray::kLengthOffset));  // Compare smis.
  __ j(above_equal, &extra, not_taken);

  // Fast case: Do the store.
  __ bind(&fast);
  // eax: value
  // ecx: key (a smi)
  // edx: receiver
  // edi: FixedArray receiver->elements
  __ mov(FieldOperand(edi, ecx, times_2, FixedArray::kHeaderSize), eax);
  // Update write barrier for the elements array address.
  __ mov(edx, Operand(eax));
  __ RecordWrite(edi, 0, edx, ecx);
  __ ret(0);
}


void KeyedStoreIC::GenerateExternalArray(MacroAssembler* masm,
                                         ExternalArrayType array_type) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : key
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------
  Label slow, check_heap_number;

  // Check that the object isn't a smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &slow);
  // Get the map from the receiver.
  __ mov(edi, FieldOperand(edx, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.  We need
  // to do this because this generic stub does not perform map checks.
  __ movzx_b(ebx, FieldOperand(edi, Map::kBitFieldOffset));
  __ test(ebx, Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &slow);
  // Check that the key is a smi.
  __ test(ecx, Immediate(kSmiTagMask));
  __ j(not_zero, &slow);
  // Get the instance type from the map of the receiver.
  __ CmpInstanceType(edi, JS_OBJECT_TYPE);
  __ j(not_equal, &slow);

  // Check that the elements array is the appropriate type of
  // ExternalArray.
  // eax: value
  // edx: receiver, a JSObject
  // ecx: key, a smi
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
  __ CheckMap(edi, Handle<Map>(Heap::MapForExternalArrayType(array_type)),
              &slow, true);

  // Check that the index is in range.
  __ mov(ebx, ecx);
  __ SmiUntag(ebx);
  __ cmp(ebx, FieldOperand(edi, ExternalArray::kLengthOffset));
  // Unsigned comparison catches both negative and too-large values.
  __ j(above_equal, &slow);

  // Handle both smis and HeapNumbers in the fast path. Go to the
  // runtime for all other kinds of values.
  // eax: value
  // edx: receiver
  // ecx: key
  // edi: elements array
  // ebx: untagged index
  __ test(eax, Immediate(kSmiTagMask));
  __ j(not_equal, &check_heap_number);
  // smi case
  __ mov(ecx, eax);  // Preserve the value in eax.  Key is no longer needed.
  __ SmiUntag(ecx);
  __ mov(edi, FieldOperand(edi, ExternalArray::kExternalPointerOffset));
  // ecx: base pointer of external storage
  switch (array_type) {
    case kExternalByteArray:
    case kExternalUnsignedByteArray:
      __ mov_b(Operand(edi, ebx, times_1, 0), ecx);
      break;
    case kExternalShortArray:
    case kExternalUnsignedShortArray:
      __ mov_w(Operand(edi, ebx, times_2, 0), ecx);
      break;
    case kExternalIntArray:
    case kExternalUnsignedIntArray:
      __ mov(Operand(edi, ebx, times_4, 0), ecx);
      break;
    case kExternalFloatArray:
      // Need to perform int-to-float conversion.
      __ push(ecx);
      __ fild_s(Operand(esp, 0));
      __ pop(ecx);
      __ fstp_s(Operand(edi, ebx, times_4, 0));
      break;
    default:
      UNREACHABLE();
      break;
  }
  __ ret(0);  // Return the original value.

  __ bind(&check_heap_number);
  // eax: value
  // edx: receiver
  // ecx: key
  // edi: elements array
  // ebx: untagged index
  __ cmp(FieldOperand(eax, HeapObject::kMapOffset),
         Immediate(Factory::heap_number_map()));
  __ j(not_equal, &slow);

  // The WebGL specification leaves the behavior of storing NaN and
  // +/-Infinity into integer arrays basically undefined. For more
  // reproducible behavior, convert these to zero.
  __ fld_d(FieldOperand(eax, HeapNumber::kValueOffset));
  __ mov(edi, FieldOperand(edi, ExternalArray::kExternalPointerOffset));
  // ebx: untagged index
  // edi: base pointer of external storage
  // top of FPU stack: value
  if (array_type == kExternalFloatArray) {
    __ fstp_s(Operand(edi, ebx, times_4, 0));
    __ ret(0);
  } else {
    // Need to perform float-to-int conversion.
    // Test the top of the FP stack for NaN.
    Label is_nan;
    __ fucomi(0);
    __ j(parity_even, &is_nan);

    if (array_type != kExternalUnsignedIntArray) {
      __ push(ecx);  // Make room on stack
      __ fistp_s(Operand(esp, 0));
      __ pop(ecx);
    } else {
      // fistp stores values as signed integers.
      // To represent the entire range, we need to store as a 64-bit
      // int and discard the high 32 bits.
      __ sub(Operand(esp), Immediate(2 * kPointerSize));
      __ fistp_d(Operand(esp, 0));
      __ pop(ecx);
      __ add(Operand(esp), Immediate(kPointerSize));
    }
    // ecx: untagged integer value
    switch (array_type) {
      case kExternalByteArray:
      case kExternalUnsignedByteArray:
        __ mov_b(Operand(edi, ebx, times_1, 0), ecx);
        break;
      case kExternalShortArray:
      case kExternalUnsignedShortArray:
        __ mov_w(Operand(edi, ebx, times_2, 0), ecx);
        break;
      case kExternalIntArray:
      case kExternalUnsignedIntArray: {
        // We also need to explicitly check for +/-Infinity. These are
        // converted to MIN_INT, but we need to be careful not to
        // confuse with legal uses of MIN_INT.
        Label not_infinity;
        // This test would apparently detect both NaN and Infinity,
        // but we've already checked for NaN using the FPU hardware
        // above.
        __ mov_w(edx, FieldOperand(eax, HeapNumber::kValueOffset + 6));
        __ and_(edx, 0x7FF0);
        __ cmp(edx, 0x7FF0);
        __ j(not_equal, &not_infinity);
        __ mov(ecx, 0);
        __ bind(&not_infinity);
        __ mov(Operand(edi, ebx, times_4, 0), ecx);
        break;
      }
      default:
        UNREACHABLE();
        break;
    }
    __ ret(0);  // Return original value.

    __ bind(&is_nan);
    __ ffree();
    __ fincstp();
    switch (array_type) {
      case kExternalByteArray:
      case kExternalUnsignedByteArray:
        __ mov_b(Operand(edi, ebx, times_1, 0), 0);
        break;
      case kExternalShortArray:
      case kExternalUnsignedShortArray:
        __ xor_(ecx, Operand(ecx));
        __ mov_w(Operand(edi, ebx, times_2, 0), ecx);
        break;
      case kExternalIntArray:
      case kExternalUnsignedIntArray:
        __ mov(Operand(edi, ebx, times_4, 0), Immediate(0));
        break;
      default:
        UNREACHABLE();
        break;
    }
    __ ret(0);  // Return the original value.
  }

  // Slow case: call runtime.
  __ bind(&slow);
  GenerateRuntimeSetProperty(masm);
}


// Defined in ic.cc.
Object* CallIC_Miss(Arguments args);

void CallIC::GenerateMegamorphic(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------
  Label number, non_number, non_string, boolean, probe, miss;

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Probe the stub cache.
  Code::Flags flags =
      Code::ComputeFlags(Code::CALL_IC, NOT_IN_LOOP, MONOMORPHIC, NORMAL, argc);
  StubCache::GenerateProbe(masm, flags, edx, ecx, ebx, eax);

  // If the stub cache probing failed, the receiver might be a value.
  // For value objects, we use the map of the prototype objects for
  // the corresponding JSValue for the cache and that is what we need
  // to probe.
  //
  // Check for number.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &number, not_taken);
  __ CmpObjectType(edx, HEAP_NUMBER_TYPE, ebx);
  __ j(not_equal, &non_number, taken);
  __ bind(&number);
  StubCompiler::GenerateLoadGlobalFunctionPrototype(
      masm, Context::NUMBER_FUNCTION_INDEX, edx);
  __ jmp(&probe);

  // Check for string.
  __ bind(&non_number);
  __ cmp(ebx, FIRST_NONSTRING_TYPE);
  __ j(above_equal, &non_string, taken);
  StubCompiler::GenerateLoadGlobalFunctionPrototype(
      masm, Context::STRING_FUNCTION_INDEX, edx);
  __ jmp(&probe);

  // Check for boolean.
  __ bind(&non_string);
  __ cmp(edx, Factory::true_value());
  __ j(equal, &boolean, not_taken);
  __ cmp(edx, Factory::false_value());
  __ j(not_equal, &miss, taken);
  __ bind(&boolean);
  StubCompiler::GenerateLoadGlobalFunctionPrototype(
      masm, Context::BOOLEAN_FUNCTION_INDEX, edx);

  // Probe the stub cache for the value object.
  __ bind(&probe);
  StubCache::GenerateProbe(masm, flags, edx, ecx, ebx, no_reg);

  // Cache miss: Jump to runtime.
  __ bind(&miss);
  GenerateMiss(masm, argc);
}


static void GenerateNormalHelper(MacroAssembler* masm,
                                 int argc,
                                 bool is_global_object,
                                 Label* miss) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- edx                 : receiver
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // Search dictionary - put result in register edi.
  __ mov(edi, edx);
  GenerateDictionaryLoad(masm, miss, edx, ecx, eax, edi, ebx, CHECK_DICTIONARY);

  // Check that the result is not a smi.
  __ test(edi, Immediate(kSmiTagMask));
  __ j(zero, miss, not_taken);

  // Check that the value is a JavaScript function, fetching its map into eax.
  __ CmpObjectType(edi, JS_FUNCTION_TYPE, eax);
  __ j(not_equal, miss, not_taken);

  // Patch the receiver on stack with the global proxy if necessary.
  if (is_global_object) {
    __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
    __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
  }

  // Invoke the function.
  ParameterCount actual(argc);
  __ InvokeFunction(edi, actual, JUMP_FUNCTION);
}


void CallIC::GenerateNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------
  Label miss, global_object, non_global_object;

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  // Check that the receiver is a valid JS object.
  __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));
  __ movzx_b(eax, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ cmp(eax, FIRST_JS_OBJECT_TYPE);
  __ j(below, &miss, not_taken);

  // If this assert fails, we have to check upper bound too.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);

  // Check for access to global object.
  __ cmp(eax, JS_GLOBAL_OBJECT_TYPE);
  __ j(equal, &global_object);
  __ cmp(eax, JS_BUILTINS_OBJECT_TYPE);
  __ j(not_equal, &non_global_object);

  // Accessing global object: Load and invoke.
  __ bind(&global_object);
  // Check that the global object does not require access checks.
  __ movzx_b(ebx, FieldOperand(ebx, Map::kBitFieldOffset));
  __ test(ebx, Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_equal, &miss, not_taken);
  GenerateNormalHelper(masm, argc, true, &miss);

  // Accessing non-global object: Check for access to global proxy.
  Label global_proxy, invoke;
  __ bind(&non_global_object);
  __ cmp(eax, JS_GLOBAL_PROXY_TYPE);
  __ j(equal, &global_proxy, not_taken);
  // Check that the non-global, non-global-proxy object does not
  // require access checks.
  __ movzx_b(ebx, FieldOperand(ebx, Map::kBitFieldOffset));
  __ test(ebx, Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_equal, &miss, not_taken);
  __ bind(&invoke);
  GenerateNormalHelper(masm, argc, false, &miss);

  // Global object proxy access: Check access rights.
  __ bind(&global_proxy);
  __ CheckAccessGlobalProxy(edx, eax, &miss);
  __ jmp(&invoke);

  // Cache miss: Jump to runtime.
  __ bind(&miss);
  GenerateMiss(masm, argc);
}


void CallIC::GenerateMiss(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- ecx                 : name
  //  -- esp[0]              : return address
  //  -- esp[(argc - n) * 4] : arg[n] (zero-based)
  //  -- ...
  //  -- esp[(argc + 1) * 4] : receiver
  // -----------------------------------

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));

  // Enter an internal frame.
  __ EnterInternalFrame();

  // Push the receiver and the name of the function.
  __ push(edx);
  __ push(ecx);

  // Call the entry.
  CEntryStub stub(1);
  __ mov(eax, Immediate(2));
  __ mov(ebx, Immediate(ExternalReference(IC_Utility(kCallIC_Miss))));
  __ CallStub(&stub);

  // Move result to edi and exit the internal frame.
  __ mov(edi, eax);
  __ LeaveInternalFrame();

  // Check if the receiver is a global object of some sort.
  Label invoke, global;
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));  // receiver
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &invoke, not_taken);
  __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ cmp(ebx, JS_GLOBAL_OBJECT_TYPE);
  __ j(equal, &global);
  __ cmp(ebx, JS_BUILTINS_OBJECT_TYPE);
  __ j(not_equal, &invoke);

  // Patch the receiver on the stack.
  __ bind(&global);
  __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
  __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);

  // Invoke the function.
  ParameterCount actual(argc);
  __ bind(&invoke);
  __ InvokeFunction(edi, actual, JUMP_FUNCTION);
}


// Defined in ic.cc.
Object* LoadIC_Miss(Arguments args);

void LoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------

  // Probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(Code::LOAD_IC,
                                         NOT_IN_LOOP,
                                         MONOMORPHIC);
  StubCache::GenerateProbe(masm, flags, eax, ecx, ebx, edx);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
}


void LoadIC::GenerateNormal(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------
  Label miss, probe, global;

  // Check that the receiver isn't a smi.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  // Check that the receiver is a valid JS object.
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(edx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ cmp(edx, FIRST_JS_OBJECT_TYPE);
  __ j(less, &miss, not_taken);

  // If this assert fails, we have to check upper bound too.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);

  // Check for access to global object (unlikely).
  __ cmp(edx, JS_GLOBAL_PROXY_TYPE);
  __ j(equal, &global, not_taken);

  // Check for non-global object that requires access check.
  __ movzx_b(ebx, FieldOperand(ebx, Map::kBitFieldOffset));
  __ test(ebx, Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &miss, not_taken);

  // Search the dictionary placing the result in eax.
  __ bind(&probe);
  GenerateDictionaryLoad(masm,
                         &miss,
                         eax,
                         ecx,
                         edx,
                         edi,
                         ebx,
                         CHECK_DICTIONARY);
  __ mov(eax, edi);
  __ ret(0);

  // Global object access: Check access rights.
  __ bind(&global);
  __ CheckAccessGlobalProxy(eax, edx, &miss);
  __ jmp(&probe);

  // Cache miss: Restore receiver from stack and jump to runtime.
  __ bind(&miss);
  GenerateMiss(masm);
}


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : receiver
  //  -- ecx    : name
  //  -- esp[0] : return address
  // -----------------------------------

  __ pop(ebx);
  __ push(eax);  // receiver
  __ push(ecx);  // name
  __ push(ebx);  // return address

  // Perform tail call to the entry.
  ExternalReference ref = ExternalReference(IC_Utility(kLoadIC_Miss));
  __ TailCallExternalReference(ref, 2, 1);
}


// One byte opcode for test eax,0xXXXXXXXX.
static const byte kTestEaxByte = 0xA9;


void LoadIC::ClearInlinedVersion(Address address) {
  // Reset the map check of the inlined inobject property load (if
  // present) to guarantee failure by holding an invalid map (the null
  // value).  The offset can be patched to anything.
  PatchInlinedLoad(address, Heap::null_value(), kMaxInt);
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

  // The map address is the last 4 bytes of the 7-byte
  // operand-immediate compare instruction, so we add 3 to get the
  // offset to the last 4 bytes.
  Address map_address = test_instruction_address + delta + 3;
  *(reinterpret_cast<Object**>(map_address)) = map;

  // The offset is in the last 4 bytes of a six byte
  // memory-to-register move instruction, so we add 2 to get the
  // offset to the last 4 bytes.
  Address offset_address =
      test_instruction_address + delta + kOffsetToLoadInstruction + 2;
  *reinterpret_cast<int*>(offset_address) = offset - kHeapObjectTag;
  return true;
}


static bool PatchInlinedMapCheck(Address address, Object* map) {
  Address test_instruction_address =
      address + Assembler::kCallTargetAddressOffset;
  // The keyed load has a fast inlined case if the IC call instruction
  // is immediately followed by a test instruction.
  if (*test_instruction_address != kTestEaxByte) return false;

  // Fetch the offset from the test instruction to the map cmp
  // instruction.  This offset is stored in the last 4 bytes of the 5
  // byte test instruction.
  Address delta_address = test_instruction_address + 1;
  int delta = *reinterpret_cast<int*>(delta_address);
  // Compute the map address.  The map address is in the last 4 bytes
  // of the 7-byte operand-immediate compare instruction, so we add 3
  // to the offset to get the map address.
  Address map_address = test_instruction_address + delta + 3;
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


// Defined in ic.cc.
Object* KeyedLoadIC_Miss(Arguments args);


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
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
  ExternalReference ref = ExternalReference(IC_Utility(kKeyedLoadIC_Miss));
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


void StoreIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- edx    : receiver
  //  -- esp[0] : return address
  // -----------------------------------

  Code::Flags flags = Code::ComputeFlags(Code::STORE_IC,
                                         NOT_IN_LOOP,
                                         MONOMORPHIC);
  StubCache::GenerateProbe(masm, flags, edx, ecx, ebx, no_reg);

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
  ExternalReference ref = ExternalReference(IC_Utility(kStoreIC_Miss));
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
  // This accepts as a receiver anything JSObject::SetElementsLength accepts
  // (currently anything except for external and pixel arrays which means
  // anything with elements of FixedArray type.), but currently is restricted
  // to JSArray.
  // Value must be a number, but only smis are accepted as the most common case.

  Label miss;

  Register receiver = edx;
  Register value = eax;
  Register scratch = ebx;

  // Check that the receiver isn't a smi.
  __ test(receiver, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  // Check that the object is a JS array.
  __ CmpObjectType(receiver, JS_ARRAY_TYPE, scratch);
  __ j(not_equal, &miss, not_taken);

  // Check that elements are FixedArray.
  __ mov(scratch, FieldOperand(receiver, JSArray::kElementsOffset));
  __ CmpObjectType(scratch, FIXED_ARRAY_TYPE, scratch);
  __ j(not_equal, &miss, not_taken);

  // Check that value is a smi.
  __ test(value, Immediate(kSmiTagMask));
  __ j(not_zero, &miss, not_taken);

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


// Defined in ic.cc.
Object* KeyedStoreIC_Miss(Arguments args);

void KeyedStoreIC::GenerateRuntimeSetProperty(MacroAssembler* masm) {
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
  __ TailCallRuntime(Runtime::kSetProperty, 3, 1);
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm) {
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
  ExternalReference ref = ExternalReference(IC_Utility(kKeyedStoreIC_Miss));
  __ TailCallExternalReference(ref, 3, 1);
}

#undef __


} }  // namespace v8::internal
