// Copyright 2013 the V8 project authors. All rights reserved.
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

#if V8_TARGET_ARCH_A64

#include "a64/assembler-a64.h"
#include "code-stubs.h"
#include "codegen.h"
#include "disasm.h"
#include "ic-inl.h"
#include "runtime.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)


// "type" holds an instance type on entry and is not clobbered.
// Generated code branch on "global_object" if type is any kind of global
// JS object.
static void GenerateGlobalInstanceTypeCheck(MacroAssembler* masm,
                                            Register type,
                                            Label* global_object) {
  __ Cmp(type, JS_GLOBAL_OBJECT_TYPE);
  __ Ccmp(type, JS_BUILTINS_OBJECT_TYPE, ZFlag, ne);
  __ Ccmp(type, JS_GLOBAL_PROXY_TYPE, ZFlag, ne);
  __ B(eq, global_object);
}


// Generated code falls through if the receiver is a regular non-global
// JS object with slow properties and no interceptors.
//
// "receiver" holds the receiver on entry and is unchanged.
// "elements" holds the property dictionary on fall through.
static void GenerateNameDictionaryReceiverCheck(MacroAssembler* masm,
                                                Register receiver,
                                                Register elements,
                                                Register scratch0,
                                                Register scratch1,
                                                Label* miss) {
  ASSERT(!AreAliased(receiver, elements, scratch0, scratch1));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, miss);

  // Check that the receiver is a valid JS object.
  // Let t be the object instance type, we want:
  //   FIRST_SPEC_OBJECT_TYPE <= t <= LAST_SPEC_OBJECT_TYPE.
  // Since LAST_SPEC_OBJECT_TYPE is the last possible instance type we only
  // check the lower bound.
  STATIC_ASSERT(LAST_TYPE == LAST_SPEC_OBJECT_TYPE);

  __ JumpIfObjectType(receiver, scratch0, scratch1, FIRST_SPEC_OBJECT_TYPE,
                      miss, lt);

  // scratch0 now contains the map of the receiver and scratch1 the object type.
  Register map = scratch0;
  Register type = scratch1;

  // Check if the receiver is a global JS object.
  GenerateGlobalInstanceTypeCheck(masm, type, miss);

  // Check that the object does not require access checks.
  __ Ldrb(scratch1, FieldMemOperand(map, Map::kBitFieldOffset));
  __ Tbnz(scratch1, Map::kIsAccessCheckNeeded, miss);
  __ Tbnz(scratch1, Map::kHasNamedInterceptor, miss);

  // Check that the properties dictionary is valid.
  __ Ldr(elements, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ Ldr(scratch1, FieldMemOperand(elements, HeapObject::kMapOffset));
  __ JumpIfNotRoot(scratch1, Heap::kHashTableMapRootIndex, miss);
}


// Helper function used from LoadIC GenerateNormal.
//
// elements: Property dictionary. It is not clobbered if a jump to the miss
//           label is done.
// name:     Property name. It is not clobbered if a jump to the miss label is
//           done
// result:   Register for the result. It is only updated if a jump to the miss
//           label is not done.
// The scratch registers need to be different from elements, name and result.
// The generated code assumes that the receiver has slow properties,
// is not a global object and does not have interceptors.
static void GenerateDictionaryLoad(MacroAssembler* masm,
                                   Label* miss,
                                   Register elements,
                                   Register name,
                                   Register result,
                                   Register scratch1,
                                   Register scratch2) {
  ASSERT(!AreAliased(elements, name, scratch1, scratch2));
  ASSERT(!AreAliased(result, scratch1, scratch2));

  Label done;

  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm,
                                                   miss,
                                                   &done,
                                                   elements,
                                                   name,
                                                   scratch1,
                                                   scratch2);

  // If probing finds an entry check that the value is a normal property.
  __ Bind(&done);

  static const int kElementsStartOffset = NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;
  static const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  __ Ldr(scratch1, FieldMemOperand(scratch2, kDetailsOffset));
  __ Tst(scratch1, Operand(Smi::FromInt(PropertyDetails::TypeField::kMask)));
  __ B(ne, miss);

  // Get the value at the masked, scaled index and return.
  __ Ldr(result,
         FieldMemOperand(scratch2, kElementsStartOffset + 1 * kPointerSize));
}


// Helper function used from StoreIC::GenerateNormal.
//
// elements: Property dictionary. It is not clobbered if a jump to the miss
//           label is done.
// name:     Property name. It is not clobbered if a jump to the miss label is
//           done
// value:    The value to store (never clobbered).
//
// The generated code assumes that the receiver has slow properties,
// is not a global object and does not have interceptors.
static void GenerateDictionaryStore(MacroAssembler* masm,
                                    Label* miss,
                                    Register elements,
                                    Register name,
                                    Register value,
                                    Register scratch1,
                                    Register scratch2) {
  ASSERT(!AreAliased(elements, name, value, scratch1, scratch2));

  Label done;

  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm,
                                                   miss,
                                                   &done,
                                                   elements,
                                                   name,
                                                   scratch1,
                                                   scratch2);

  // If probing finds an entry in the dictionary check that the value
  // is a normal property that is not read only.
  __ Bind(&done);

  static const int kElementsStartOffset = NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;
  static const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  static const int kTypeAndReadOnlyMask =
      PropertyDetails::TypeField::kMask |
      PropertyDetails::AttributesField::encode(READ_ONLY);
  __ Ldrsw(scratch1, UntagSmiFieldMemOperand(scratch2, kDetailsOffset));
  __ Tst(scratch1, kTypeAndReadOnlyMask);
  __ B(ne, miss);

  // Store the value at the masked, scaled index and return.
  static const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ Add(scratch2, scratch2, kValueOffset - kHeapObjectTag);
  __ Str(value, MemOperand(scratch2));

  // Update the write barrier. Make sure not to clobber the value.
  __ Mov(scratch1, value);
  __ RecordWrite(
      elements, scratch2, scratch1, kLRHasNotBeenSaved, kDontSaveFPRegs);
}


// Checks the receiver for special cases (value type, slow case bits).
// Falls through for regular JS object and return the map of the
// receiver in 'map_scratch' if the receiver is not a SMI.
static void GenerateKeyedLoadReceiverCheck(MacroAssembler* masm,
                                           Register receiver,
                                           Register map_scratch,
                                           Register scratch,
                                           int interceptor_bit,
                                           Label* slow) {
  ASSERT(!AreAliased(map_scratch, scratch));

  // Check that the object isn't a smi.
  __ JumpIfSmi(receiver, slow);
  // Get the map of the receiver.
  __ Ldr(map_scratch, FieldMemOperand(receiver, HeapObject::kMapOffset));
  // Check bit field.
  __ Ldrb(scratch, FieldMemOperand(map_scratch, Map::kBitFieldOffset));
  __ Tbnz(scratch, Map::kIsAccessCheckNeeded, slow);
  __ Tbnz(scratch, interceptor_bit, slow);

  // Check that the object is some kind of JS object EXCEPT JS Value type.
  // In the case that the object is a value-wrapper object, we enter the
  // runtime system to make sure that indexing into string objects work
  // as intended.
  STATIC_ASSERT(JS_OBJECT_TYPE > JS_VALUE_TYPE);
  __ Ldrb(scratch, FieldMemOperand(map_scratch, Map::kInstanceTypeOffset));
  __ Cmp(scratch, JS_OBJECT_TYPE);
  __ B(lt, slow);
}


// Loads an indexed element from a fast case array.
// If not_fast_array is NULL, doesn't perform the elements map check.
//
// receiver     - holds the receiver on entry.
//                Unchanged unless 'result' is the same register.
//
// key          - holds the smi key on entry.
//                Unchanged unless 'result' is the same register.
//
// elements     - holds the elements of the receiver on exit.
//
// elements_map - holds the elements map on exit if the not_fast_array branch is
//                taken. Otherwise, this is used as a scratch register.
//
// result       - holds the result on exit if the load succeeded.
//                Allowed to be the the same as 'receiver' or 'key'.
//                Unchanged on bailout so 'receiver' and 'key' can be safely
//                used by further computation.
static void GenerateFastArrayLoad(MacroAssembler* masm,
                                  Register receiver,
                                  Register key,
                                  Register elements,
                                  Register elements_map,
                                  Register scratch2,
                                  Register result,
                                  Label* not_fast_array,
                                  Label* slow) {
  ASSERT(!AreAliased(receiver, key, elements, elements_map, scratch2));

  // Check for fast array.
  __ Ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  if (not_fast_array != NULL) {
    // Check that the object is in fast mode and writable.
    __ Ldr(elements_map, FieldMemOperand(elements, HeapObject::kMapOffset));
    __ JumpIfNotRoot(elements_map, Heap::kFixedArrayMapRootIndex,
                     not_fast_array);
  } else {
    __ AssertFastElements(elements);
  }

  // The elements_map register is only used for the not_fast_array path, which
  // was handled above. From this point onward it is a scratch register.
  Register scratch1 = elements_map;

  // Check that the key (index) is within bounds.
  __ Ldr(scratch1, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ Cmp(key, scratch1);
  __ B(hs, slow);

  // Fast case: Do the load.
  __ Add(scratch1, elements, FixedArray::kHeaderSize - kHeapObjectTag);
  __ SmiUntag(scratch2, key);
  __ Ldr(scratch2, MemOperand(scratch1, scratch2, LSL, kPointerSizeLog2));

  // In case the loaded value is the_hole we have to consult GetProperty
  // to ensure the prototype chain is searched.
  __ JumpIfRoot(scratch2, Heap::kTheHoleValueRootIndex, slow);

  // Move the value to the result register.
  // 'result' can alias with 'receiver' or 'key' but these two must be
  // preserved if we jump to 'slow'.
  __ Mov(result, scratch2);
}


// Checks whether a key is an array index string or a unique name.
// Falls through if a key is a unique name.
// The map of the key is returned in 'map_scratch'.
// If the jump to 'index_string' is done the hash of the key is left
// in 'hash_scratch'.
static void GenerateKeyNameCheck(MacroAssembler* masm,
                                 Register key,
                                 Register map_scratch,
                                 Register hash_scratch,
                                 Label* index_string,
                                 Label* not_unique) {
  ASSERT(!AreAliased(key, map_scratch, hash_scratch));

  // Is the key a name?
  Label unique;
  __ JumpIfObjectType(key, map_scratch, hash_scratch, LAST_UNIQUE_NAME_TYPE,
                      not_unique, hi);
  STATIC_ASSERT(LAST_UNIQUE_NAME_TYPE == FIRST_NONSTRING_TYPE);
  __ B(eq, &unique);

  // Is the string an array index with cached numeric value?
  __ Ldr(hash_scratch.W(), FieldMemOperand(key, Name::kHashFieldOffset));
  __ TestAndBranchIfAllClear(hash_scratch,
                             Name::kContainsCachedArrayIndexMask,
                             index_string);

  // Is the string internalized? We know it's a string, so a single bit test is
  // enough.
  __ Ldrb(hash_scratch, FieldMemOperand(map_scratch, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0);
  __ TestAndBranchIfAnySet(hash_scratch, kIsNotInternalizedMask, not_unique);

  __ Bind(&unique);
  // Fall through if the key is a unique name.
}


// Neither 'object' nor 'key' are modified by this function.
//
// If the 'unmapped_case' or 'slow_case' exit is taken, the 'map' register is
// left with the object's elements map. Otherwise, it is used as a scratch
// register.
static MemOperand GenerateMappedArgumentsLookup(MacroAssembler* masm,
                                                Register object,
                                                Register key,
                                                Register map,
                                                Register scratch1,
                                                Register scratch2,
                                                Label* unmapped_case,
                                                Label* slow_case) {
  ASSERT(!AreAliased(object, key, map, scratch1, scratch2));

  Heap* heap = masm->isolate()->heap();

  // Check that the receiver is a JSObject. Because of the elements
  // map check later, we do not need to check for interceptors or
  // whether it requires access checks.
  __ JumpIfSmi(object, slow_case);
  // Check that the object is some kind of JSObject.
  __ JumpIfObjectType(object, map, scratch1, FIRST_JS_RECEIVER_TYPE,
                      slow_case, lt);

  // Check that the key is a positive smi.
  __ JumpIfNotSmi(key, slow_case);
  __ Tbnz(key, kXSignBit, slow_case);

  // Load the elements object and check its map.
  Handle<Map> arguments_map(heap->non_strict_arguments_elements_map());
  __ Ldr(map, FieldMemOperand(object, JSObject::kElementsOffset));
  __ CheckMap(map, scratch1, arguments_map, slow_case, DONT_DO_SMI_CHECK);

  // Check if element is in the range of mapped arguments. If not, jump
  // to the unmapped lookup.
  __ Ldr(scratch1, FieldMemOperand(map, FixedArray::kLengthOffset));
  __ Sub(scratch1, scratch1, Operand(Smi::FromInt(2)));
  __ Cmp(key, scratch1);
  __ B(hs, unmapped_case);

  // Load element index and check whether it is the hole.
  static const int offset =
      FixedArray::kHeaderSize + 2 * kPointerSize - kHeapObjectTag;

  __ Add(scratch1, map, offset);
  __ SmiUntag(scratch2, key);
  __ Ldr(scratch1, MemOperand(scratch1, scratch2, LSL, kPointerSizeLog2));
  __ JumpIfRoot(scratch1, Heap::kTheHoleValueRootIndex, unmapped_case);

  // Load value from context and return it.
  __ Ldr(scratch2, FieldMemOperand(map, FixedArray::kHeaderSize));
  __ SmiUntag(scratch1);
  __ Add(scratch2, scratch2, Context::kHeaderSize - kHeapObjectTag);
  return MemOperand(scratch2, scratch1, LSL, kPointerSizeLog2);
}


// The 'parameter_map' register must be loaded with the parameter map of the
// arguments object and is overwritten.
static MemOperand GenerateUnmappedArgumentsLookup(MacroAssembler* masm,
                                                  Register key,
                                                  Register parameter_map,
                                                  Register scratch,
                                                  Label* slow_case) {
  ASSERT(!AreAliased(key, parameter_map, scratch));

  // Element is in arguments backing store, which is referenced by the
  // second element of the parameter_map.
  const int kBackingStoreOffset = FixedArray::kHeaderSize + kPointerSize;
  Register backing_store = parameter_map;
  __ Ldr(backing_store, FieldMemOperand(parameter_map, kBackingStoreOffset));
  Handle<Map> fixed_array_map(masm->isolate()->heap()->fixed_array_map());
  __ CheckMap(
      backing_store, scratch, fixed_array_map, slow_case, DONT_DO_SMI_CHECK);
  __ Ldr(scratch, FieldMemOperand(backing_store, FixedArray::kLengthOffset));
  __ Cmp(key, scratch);
  __ B(hs, slow_case);

  __ Add(backing_store,
         backing_store,
         FixedArray::kHeaderSize - kHeapObjectTag);
  __ SmiUntag(scratch, key);
  return MemOperand(backing_store, scratch, LSL, kPointerSizeLog2);
}


void LoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x2    : name
  //  -- lr    : return address
  //  -- x0    : receiver
  // -----------------------------------

  // Probe the stub cache.
  Code::Flags flags = Code::ComputeHandlerFlags(Code::LOAD_IC);
  masm->isolate()->stub_cache()->GenerateProbe(
      masm, flags, x0, x2, x3, x4, x5, x6);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
}


void LoadIC::GenerateNormal(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x2    : name
  //  -- lr    : return address
  //  -- x0    : receiver
  // -----------------------------------
  Label miss;

  GenerateNameDictionaryReceiverCheck(masm, x0, x1, x3, x4, &miss);

  // x1 now holds the property dictionary.
  GenerateDictionaryLoad(masm, &miss, x1, x2, x0, x3, x4);
  __ Ret();

  // Cache miss: Jump to runtime.
  __ Bind(&miss);
  GenerateMiss(masm);
}


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x2    : name
  //  -- lr    : return address
  //  -- x0    : receiver
  // -----------------------------------
  Isolate* isolate = masm->isolate();
  ASM_LOCATION("LoadIC::GenerateMiss");

  __ IncrementCounter(isolate->counters()->load_miss(), 1, x3, x4);

  // TODO(jbramley): Does the target actually expect an argument in x3, or is
  // this inherited from ARM's push semantics?
  __ Mov(x3, x0);
  __ Push(x3, x2);

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kLoadIC_Miss), isolate);
  __ TailCallExternalReference(ref, 2, 1);
}


void LoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- x2    : name
  //  -- lr    : return address
  //  -- x0    : receiver
  // -----------------------------------

  // TODO(jbramley): Does the target actually expect an argument in x3, or is
  // this inherited from ARM's push semantics?
  __ Mov(x3, x0);
  __ Push(x3, x2);

  __ TailCallRuntime(Runtime::kGetProperty, 2, 1);
}


void KeyedLoadIC::GenerateNonStrictArguments(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- x0     : key
  //  -- x1     : receiver
  // -----------------------------------
  Register result = x0;
  Register key = x0;
  Register receiver = x1;
  Label miss, unmapped;

  Register map_scratch = x2;
  MemOperand mapped_location = GenerateMappedArgumentsLookup(
      masm, receiver, key, map_scratch, x3, x4, &unmapped, &miss);
  __ Ldr(result, mapped_location);
  __ Ret();

  __ Bind(&unmapped);
  // Parameter map is left in map_scratch when a jump on unmapped is done.
  MemOperand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, key, map_scratch, x3, &miss);
  __ Ldr(x2, unmapped_location);
  __ JumpIfRoot(x2, Heap::kTheHoleValueRootIndex, &miss);
  // Move the result in x0. x0 must be preserved on miss.
  __ Mov(result, x2);
  __ Ret();

  __ Bind(&miss);
  GenerateMiss(masm);
}


void KeyedStoreIC::GenerateNonStrictArguments(MacroAssembler* masm) {
  ASM_LOCATION("KeyedStoreIC::GenerateNonStrictArguments");
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- x0     : value
  //  -- x1     : key
  //  -- x2     : receiver
  // -----------------------------------

  Label slow, notin;

  Register value = x0;
  Register key = x1;
  Register receiver = x2;
  Register map = x3;

  // These registers are used by GenerateMappedArgumentsLookup to build a
  // MemOperand. They are live for as long as the MemOperand is live.
  Register mapped1 = x4;
  Register mapped2 = x5;

  MemOperand mapped =
      GenerateMappedArgumentsLookup(masm, receiver, key, map,
                                    mapped1, mapped2,
                                    &notin, &slow);
  Operand mapped_offset = mapped.OffsetAsOperand();
  __ Str(value, mapped);
  __ Add(x10, mapped.base(), mapped_offset);
  __ Mov(x11, value);
  __ RecordWrite(mapped.base(), x10, x11, kLRHasNotBeenSaved, kDontSaveFPRegs);
  __ Ret();

  __ Bind(&notin);

  // These registers are used by GenerateMappedArgumentsLookup to build a
  // MemOperand. They are live for as long as the MemOperand is live.
  Register unmapped1 = map;   // This is assumed to alias 'map'.
  Register unmapped2 = x4;
  MemOperand unmapped =
      GenerateUnmappedArgumentsLookup(masm, key, unmapped1, unmapped2, &slow);
  Operand unmapped_offset = unmapped.OffsetAsOperand();
  __ Str(value, unmapped);
  __ Add(x10, unmapped.base(), unmapped_offset);
  __ Mov(x11, value);
  __ RecordWrite(unmapped.base(), x10, x11,
                 kLRHasNotBeenSaved, kDontSaveFPRegs);
  __ Ret();
  __ Bind(&slow);
  GenerateMiss(masm);
}


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- x0     : key
  //  -- x1     : receiver
  // -----------------------------------
  Isolate* isolate = masm->isolate();

  __ IncrementCounter(isolate->counters()->keyed_load_miss(), 1, x10, x11);

  __ Push(x1, x0);

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kKeyedLoadIC_Miss), isolate);

  __ TailCallExternalReference(ref, 2, 1);
}


void KeyedLoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- x0     : key
  //  -- x1     : receiver
  // -----------------------------------
  Register key = x0;
  Register receiver = x1;

  __ Push(receiver, key);
  __ TailCallRuntime(Runtime::kKeyedGetProperty, 2, 1);
}


static void GenerateKeyedLoadWithSmiKey(MacroAssembler* masm,
                                        Register key,
                                        Register receiver,
                                        Register scratch1,
                                        Register scratch2,
                                        Register scratch3,
                                        Register scratch4,
                                        Register scratch5,
                                        Label *slow) {
  ASSERT(!AreAliased(
      key, receiver, scratch1, scratch2, scratch3, scratch4, scratch5));

  Isolate* isolate = masm->isolate();
  Label check_number_dictionary;
  // If we can load the value, it should be returned in x0.
  Register result = x0;

  GenerateKeyedLoadReceiverCheck(
      masm, receiver, scratch1, scratch2, Map::kHasIndexedInterceptor, slow);

  // Check the receiver's map to see if it has fast elements.
  __ CheckFastElements(scratch1, scratch2, &check_number_dictionary);

  GenerateFastArrayLoad(
      masm, receiver, key, scratch3, scratch2, scratch1, result, NULL, slow);
  __ IncrementCounter(
      isolate->counters()->keyed_load_generic_smi(), 1, scratch1, scratch2);
  __ Ret();

  __ Bind(&check_number_dictionary);
  __ Ldr(scratch3, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ Ldr(scratch2, FieldMemOperand(scratch3, JSObject::kMapOffset));

  // Check whether we have a number dictionary.
  __ JumpIfNotRoot(scratch2, Heap::kHashTableMapRootIndex, slow);

  __ LoadFromNumberDictionary(
      slow, scratch3, key, result, scratch1, scratch2, scratch4, scratch5);
  __ Ret();
}

static void GenerateKeyedLoadWithNameKey(MacroAssembler* masm,
                                         Register key,
                                         Register receiver,
                                         Register scratch1,
                                         Register scratch2,
                                         Register scratch3,
                                         Register scratch4,
                                         Register scratch5,
                                         Label *slow) {
  ASSERT(!AreAliased(
      key, receiver, scratch1, scratch2, scratch3, scratch4, scratch5));

  Isolate* isolate = masm->isolate();
  Label probe_dictionary, property_array_property;
  // If we can load the value, it should be returned in x0.
  Register result = x0;

  GenerateKeyedLoadReceiverCheck(
      masm, receiver, scratch1, scratch2, Map::kHasNamedInterceptor, slow);

  // If the receiver is a fast-case object, check the keyed lookup cache.
  // Otherwise probe the dictionary.
  __ Ldr(scratch2, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ Ldr(scratch3, FieldMemOperand(scratch2, HeapObject::kMapOffset));
  __ JumpIfRoot(scratch3, Heap::kHashTableMapRootIndex, &probe_dictionary);

  // We keep the map of the receiver in scratch1.
  Register receiver_map = scratch1;

  // Load the map of the receiver, compute the keyed lookup cache hash
  // based on 32 bits of the map pointer and the name hash.
  __ Ldr(receiver_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Mov(scratch2, Operand(receiver_map, ASR, KeyedLookupCache::kMapHashShift));
  __ Ldr(scratch3.W(), FieldMemOperand(key, Name::kHashFieldOffset));
  __ Eor(scratch2, scratch2, Operand(scratch3, ASR, Name::kHashShift));
  int mask = KeyedLookupCache::kCapacityMask & KeyedLookupCache::kHashMask;
  __ And(scratch2, scratch2, mask);

  // Load the key (consisting of map and unique name) from the cache and
  // check for match.
  Label load_in_object_property;
  static const int kEntriesPerBucket = KeyedLookupCache::kEntriesPerBucket;
  Label hit_on_nth_entry[kEntriesPerBucket];
  ExternalReference cache_keys =
      ExternalReference::keyed_lookup_cache_keys(isolate);

  __ Mov(scratch3, Operand(cache_keys));
  __ Add(scratch3, scratch3, Operand(scratch2, LSL, kPointerSizeLog2 + 1));

  for (int i = 0; i < kEntriesPerBucket - 1; i++) {
    Label try_next_entry;
    // Load map and make scratch3 pointing to the next entry.
    __ Ldr(scratch4, MemOperand(scratch3, kPointerSize * 2, PostIndex));
    __ Cmp(receiver_map, scratch4);
    __ B(ne, &try_next_entry);
    __ Ldr(scratch4, MemOperand(scratch3, -kPointerSize));  // Load name
    __ Cmp(key, scratch4);
    __ B(eq, &hit_on_nth_entry[i]);
    __ Bind(&try_next_entry);
  }

  // Last entry.
  __ Ldr(scratch4, MemOperand(scratch3, kPointerSize, PostIndex));
  __ Cmp(receiver_map, scratch4);
  __ B(ne, slow);
  __ Ldr(scratch4, MemOperand(scratch3));
  __ Cmp(key, scratch4);
  __ B(ne, slow);

  // Get field offset.
  ExternalReference cache_field_offsets =
      ExternalReference::keyed_lookup_cache_field_offsets(isolate);

  // Hit on nth entry.
  for (int i = kEntriesPerBucket - 1; i >= 0; i--) {
    __ Bind(&hit_on_nth_entry[i]);
    __ Mov(scratch3, Operand(cache_field_offsets));
    if (i != 0) {
      __ Add(scratch2, scratch2, i);
    }
    __ Ldr(scratch4.W(), MemOperand(scratch3, scratch2, LSL, 2));
    __ Ldrb(scratch5,
            FieldMemOperand(receiver_map, Map::kInObjectPropertiesOffset));
    __ Subs(scratch4, scratch4, scratch5);
    __ B(ge, &property_array_property);
    if (i != 0) {
      __ B(&load_in_object_property);
    }
  }

  // Load in-object property.
  __ Bind(&load_in_object_property);
  __ Ldrb(scratch5, FieldMemOperand(receiver_map, Map::kInstanceSizeOffset));
  __ Add(scratch5, scratch5, scratch4);  // Index from start of object.
  __ Sub(receiver, receiver, kHeapObjectTag);  // Remove the heap tag.
  __ Ldr(result, MemOperand(receiver, scratch5, LSL, kPointerSizeLog2));
  __ IncrementCounter(isolate->counters()->keyed_load_generic_lookup_cache(),
                      1, scratch1, scratch2);
  __ Ret();

  // Load property array property.
  __ Bind(&property_array_property);
  __ Ldr(scratch1, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ Add(scratch1, scratch1, FixedArray::kHeaderSize - kHeapObjectTag);
  __ Ldr(result, MemOperand(scratch1, scratch4, LSL, kPointerSizeLog2));
  __ IncrementCounter(isolate->counters()->keyed_load_generic_lookup_cache(),
                      1, scratch1, scratch2);
  __ Ret();

  // Do a quick inline probe of the receiver's dictionary, if it exists.
  __ Bind(&probe_dictionary);
  __ Ldr(scratch1, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Ldrb(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  GenerateGlobalInstanceTypeCheck(masm, scratch1, slow);
  // Load the property.
  GenerateDictionaryLoad(masm, slow, scratch2, key, result, scratch1, scratch3);
  __ IncrementCounter(isolate->counters()->keyed_load_generic_symbol(),
                      1, scratch1, scratch2);
  __ Ret();
}


void KeyedLoadIC::GenerateGeneric(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- x0     : key
  //  -- x1     : receiver
  // -----------------------------------
  Label slow, check_name, index_smi, index_name;

  Register key = x0;
  Register receiver = x1;

  __ JumpIfNotSmi(key, &check_name);
  __ Bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from below
  // where a numeric string is converted to a smi.
  GenerateKeyedLoadWithSmiKey(masm, key, receiver, x2, x3, x4, x5, x6, &slow);

  // Slow case, key and receiver still in x0 and x1.
  __ Bind(&slow);
  __ IncrementCounter(
      masm->isolate()->counters()->keyed_load_generic_slow(), 1, x2, x3);
  GenerateRuntimeGetProperty(masm);

  __ Bind(&check_name);
  GenerateKeyNameCheck(masm, key, x2, x3, &index_name, &slow);

  GenerateKeyedLoadWithNameKey(masm, key, receiver, x2, x3, x4, x5, x6, &slow);

  __ Bind(&index_name);
  __ IndexFromHash(x3, key);
  // Now jump to the place where smi keys are handled.
  __ B(&index_smi);
}


void KeyedLoadIC::GenerateString(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- x0     : key (index)
  //  -- x1     : receiver
  // -----------------------------------
  Label miss;

  Register index = x0;
  Register receiver = x1;
  Register result = x0;
  Register scratch = x3;

  StringCharAtGenerator char_at_generator(receiver,
                                          index,
                                          scratch,
                                          result,
                                          &miss,  // When not a string.
                                          &miss,  // When not a number.
                                          &miss,  // When index out of range.
                                          STRING_INDEX_IS_ARRAY_INDEX);
  char_at_generator.GenerateFast(masm);
  __ Ret();

  StubRuntimeCallHelper call_helper;
  char_at_generator.GenerateSlow(masm, call_helper);

  __ Bind(&miss);
  GenerateMiss(masm);
}


void KeyedLoadIC::GenerateIndexedInterceptor(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- x0     : key
  //  -- x1     : receiver
  // -----------------------------------
  Label slow;
  Register key = x0;
  Register receiver = x1;

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &slow);

  // Check that the key is an array index, that is Uint32.
  __ TestAndBranchIfAnySet(key, kSmiTagMask | kSmiSignMask, &slow);

  // Get the map of the receiver.
  Register map = x2;
  __ Ldr(map, FieldMemOperand(receiver, HeapObject::kMapOffset));

  // Check that it has indexed interceptor and access checks
  // are not enabled for this object.
  __ Ldrb(x3, FieldMemOperand(map, Map::kBitFieldOffset));
  ASSERT(kSlowCaseBitFieldMask ==
      ((1 << Map::kIsAccessCheckNeeded) | (1 << Map::kHasIndexedInterceptor)));
  __ Tbnz(x3, Map::kIsAccessCheckNeeded, &slow);
  __ Tbz(x3, Map::kHasIndexedInterceptor, &slow);

  // Everything is fine, call runtime.
  __ Push(receiver, key);
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(kKeyedLoadPropertyWithInterceptor),
                        masm->isolate()),
      2,
      1);

  __ Bind(&slow);
  GenerateMiss(masm);
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm) {
  ASM_LOCATION("KeyedStoreIC::GenerateMiss");
  // ---------- S t a t e --------------
  //  -- x0     : value
  //  -- x1     : key
  //  -- x2     : receiver
  //  -- lr     : return address
  // -----------------------------------

  // Push receiver, key and value for runtime call.
  __ Push(x2, x1, x0);

  ExternalReference ref =
      ExternalReference(IC_Utility(kKeyedStoreIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void KeyedStoreIC::GenerateSlow(MacroAssembler* masm) {
  ASM_LOCATION("KeyedStoreIC::GenerateSlow");
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- x0     : value
  //  -- x1     : key
  //  -- x2     : receiver
  // -----------------------------------

  // Push receiver, key and value for runtime call.
  __ Push(x2, x1, x0);

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  ExternalReference ref =
      ExternalReference(IC_Utility(kKeyedStoreIC_Slow), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void KeyedStoreIC::GenerateRuntimeSetProperty(MacroAssembler* masm,
                                              StrictModeFlag strict_mode) {
  ASM_LOCATION("KeyedStoreIC::GenerateRuntimeSetProperty");
  // ---------- S t a t e --------------
  //  -- x0     : value
  //  -- x1     : key
  //  -- x2     : receiver
  //  -- lr     : return address
  // -----------------------------------

  // Push receiver, key and value for runtime call.
  __ Push(x2, x1, x0);

  // Push PropertyAttributes(NONE) and strict_mode for runtime call.
  STATIC_ASSERT(NONE == 0);
  __ Mov(x10, Operand(Smi::FromInt(strict_mode)));
  __ Push(xzr, x10);

  __ TailCallRuntime(Runtime::kSetProperty, 5, 1);
}


static void KeyedStoreGenerateGenericHelper(
    MacroAssembler* masm,
    Label* fast_object,
    Label* fast_double,
    Label* slow,
    KeyedStoreCheckMap check_map,
    KeyedStoreIncrementLength increment_length,
    Register value,
    Register key,
    Register receiver,
    Register receiver_map,
    Register elements_map,
    Register elements) {
  ASSERT(!AreAliased(
      value, key, receiver, receiver_map, elements_map, elements, x10, x11));

  Label transition_smi_elements;
  Label transition_double_elements;
  Label fast_double_without_map_check;
  Label non_double_value;
  Label finish_store;

  __ Bind(fast_object);
  if (check_map == kCheckMap) {
    __ Ldr(elements_map, FieldMemOperand(elements, HeapObject::kMapOffset));
    __ Cmp(elements_map,
           Operand(masm->isolate()->factory()->fixed_array_map()));
    __ B(ne, fast_double);
  }

  // HOLECHECK: guards "A[i] = V"
  // We have to go to the runtime if the current value is the hole because there
  // may be a callback on the element.
  Label holecheck_passed;
  // TODO(all): This address calculation is repeated later (for the store
  // itself). We should keep the result to avoid doing the work twice.
  __ Add(x10, elements, FixedArray::kHeaderSize - kHeapObjectTag);
  __ Add(x10, x10, Operand::UntagSmiAndScale(key, kPointerSizeLog2));
  __ Ldr(x11, MemOperand(x10));
  __ JumpIfNotRoot(x11, Heap::kTheHoleValueRootIndex, &holecheck_passed);
  __ JumpIfDictionaryInPrototypeChain(receiver, elements_map, x10, slow);
  __ bind(&holecheck_passed);

  // Smi stores don't require further checks.
  __ JumpIfSmi(value, &finish_store);

  // Escape to elements kind transition case.
  __ CheckFastObjectElements(receiver_map, x10, &transition_smi_elements);

  __ Bind(&finish_store);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ Add(x10, key, Operand(Smi::FromInt(1)));
    __ Str(x10, FieldMemOperand(receiver, JSArray::kLengthOffset));
  }

  Register address = x11;
  __ Add(address, elements, FixedArray::kHeaderSize - kHeapObjectTag);
  __ Add(address, address, Operand::UntagSmiAndScale(key, kPointerSizeLog2));
  __ Str(value, MemOperand(address));

  Label dont_record_write;
  __ JumpIfSmi(value, &dont_record_write);

  // Update write barrier for the elements array address.
  __ Mov(x10, value);  // Preserve the value which is returned.
  __ RecordWrite(elements,
                 address,
                 x10,
                 kLRHasNotBeenSaved,
                 kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET,
                 OMIT_SMI_CHECK);

  __ Bind(&dont_record_write);
  __ Ret();


  __ Bind(fast_double);
  if (check_map == kCheckMap) {
    // Check for fast double array case. If this fails, call through to the
    // runtime.
    __ JumpIfNotRoot(elements_map, Heap::kFixedDoubleArrayMapRootIndex, slow);
  }

  // HOLECHECK: guards "A[i] double hole?"
  // We have to see if the double version of the hole is present. If so go to
  // the runtime.
  // TODO(all): This address calculation was done earlier. We should keep the
  // result to avoid doing the work twice.
  __ Add(x10, elements, FixedDoubleArray::kHeaderSize - kHeapObjectTag);
  __ Add(x10, x10, Operand::UntagSmiAndScale(key, kPointerSizeLog2));
  __ Ldr(x11, MemOperand(x10));
  __ CompareAndBranch(x11, kHoleNanInt64, ne, &fast_double_without_map_check);
  __ JumpIfDictionaryInPrototypeChain(receiver, elements_map, x10, slow);

  __ Bind(&fast_double_without_map_check);
  __ StoreNumberToDoubleElements(value,
                                 key,
                                 elements,
                                 x10,
                                 d0,
                                 d1,
                                 &transition_double_elements);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ Add(x10, key, Operand(Smi::FromInt(1)));
    __ Str(x10, FieldMemOperand(receiver, JSArray::kLengthOffset));
  }
  __ Ret();


  __ Bind(&transition_smi_elements);
  // Transition the array appropriately depending on the value type.
  __ Ldr(x10, FieldMemOperand(value, HeapObject::kMapOffset));
  __ JumpIfNotRoot(x10, Heap::kHeapNumberMapRootIndex, &non_double_value);

  // Value is a double. Transition FAST_SMI_ELEMENTS ->
  // FAST_DOUBLE_ELEMENTS and complete the store.
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                         FAST_DOUBLE_ELEMENTS,
                                         receiver_map,
                                         x10,
                                         slow);
  ASSERT(receiver_map.Is(x3));  // Transition code expects map in x3.
  AllocationSiteMode mode = AllocationSite::GetMode(FAST_SMI_ELEMENTS,
                                                    FAST_DOUBLE_ELEMENTS);
  ElementsTransitionGenerator::GenerateSmiToDouble(masm, mode, slow);
  __ Ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ B(&fast_double_without_map_check);

  __ Bind(&non_double_value);
  // Value is not a double, FAST_SMI_ELEMENTS -> FAST_ELEMENTS.
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                         FAST_ELEMENTS,
                                         receiver_map,
                                         x10,
                                         slow);
  ASSERT(receiver_map.Is(x3));  // Transition code expects map in x3.
  mode = AllocationSite::GetMode(FAST_SMI_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateMapChangeElementsTransition(masm, mode,
                                                                   slow);
  __ Ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ B(&finish_store);

  __ Bind(&transition_double_elements);
  // Elements are FAST_DOUBLE_ELEMENTS, but value is an Object that's not a
  // HeapNumber. Make sure that the receiver is a Array with FAST_ELEMENTS and
  // transition array from FAST_DOUBLE_ELEMENTS to FAST_ELEMENTS
  __ LoadTransitionedArrayMapConditional(FAST_DOUBLE_ELEMENTS,
                                         FAST_ELEMENTS,
                                         receiver_map,
                                         x10,
                                         slow);
  ASSERT(receiver_map.Is(x3));  // Transition code expects map in x3.
  mode = AllocationSite::GetMode(FAST_DOUBLE_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateDoubleToObject(masm, mode, slow);
  __ Ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ B(&finish_store);
}


void KeyedStoreIC::GenerateGeneric(MacroAssembler* masm,
                                   StrictModeFlag strict_mode) {
  ASM_LOCATION("KeyedStoreIC::GenerateGeneric");
  // ---------- S t a t e --------------
  //  -- x0     : value
  //  -- x1     : key
  //  -- x2     : receiver
  //  -- lr     : return address
  // -----------------------------------
  Label slow;
  Label array;
  Label fast_object;
  Label extra;
  Label fast_object_grow;
  Label fast_double_grow;
  Label fast_double;

  Register value = x0;
  Register key = x1;
  Register receiver = x2;
  Register receiver_map = x3;
  Register elements = x4;
  Register elements_map = x5;

  __ JumpIfNotSmi(key, &slow);
  __ JumpIfSmi(receiver, &slow);
  __ Ldr(receiver_map, FieldMemOperand(receiver, HeapObject::kMapOffset));

  // Check that the receiver does not require access checks and is not observed.
  // The generic stub does not perform map checks or handle observed objects.
  __ Ldrb(x10, FieldMemOperand(receiver_map, Map::kBitFieldOffset));
  __ TestAndBranchIfAnySet(
      x10, (1 << Map::kIsAccessCheckNeeded) | (1 << Map::kIsObserved), &slow);

  // Check if the object is a JS array or not.
  Register instance_type = x10;
  __ CompareInstanceType(receiver_map, instance_type, JS_ARRAY_TYPE);
  __ B(eq, &array);
  // Check that the object is some kind of JSObject.
  __ Cmp(instance_type, FIRST_JS_OBJECT_TYPE);
  __ B(lt, &slow);

  // Object case: Check key against length in the elements array.
  __ Ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  // Check array bounds. Both the key and the length of FixedArray are smis.
  __ Ldrsw(x10, UntagSmiFieldMemOperand(elements, FixedArray::kLengthOffset));
  __ Cmp(x10, Operand::UntagSmi(key));
  __ B(hi, &fast_object);


  __ Bind(&slow);
  // Slow case, handle jump to runtime.
  // Live values:
  //  x0: value
  //  x1: key
  //  x2: receiver
  GenerateRuntimeSetProperty(masm, strict_mode);


  __ Bind(&extra);
  // Extra capacity case: Check if there is extra capacity to
  // perform the store and update the length. Used for adding one
  // element to the array by writing to array[array.length].

  // Check for room in the elements backing store.
  // Both the key and the length of FixedArray are smis.
  __ Ldrsw(x10, UntagSmiFieldMemOperand(elements, FixedArray::kLengthOffset));
  __ Cmp(x10, Operand::UntagSmi(key));
  __ B(ls, &slow);

  __ Ldr(elements_map, FieldMemOperand(elements, HeapObject::kMapOffset));
  __ Cmp(elements_map, Operand(masm->isolate()->factory()->fixed_array_map()));
  __ B(eq, &fast_object_grow);
  __ Cmp(elements_map,
         Operand(masm->isolate()->factory()->fixed_double_array_map()));
  __ B(eq, &fast_double_grow);
  __ B(&slow);


  __ Bind(&array);
  // Array case: Get the length and the elements array from the JS
  // array. Check that the array is in fast mode (and writable); if it
  // is the length is always a smi.

  __ Ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));

  // Check the key against the length in the array.
  __ Ldrsw(x10, UntagSmiFieldMemOperand(receiver, JSArray::kLengthOffset));
  __ Cmp(x10, Operand::UntagSmi(key));
  __ B(eq, &extra);  // We can handle the case where we are appending 1 element.
  __ B(lo, &slow);

  KeyedStoreGenerateGenericHelper(masm, &fast_object, &fast_double,
                                  &slow, kCheckMap, kDontIncrementLength,
                                  value, key, receiver, receiver_map,
                                  elements_map, elements);
  KeyedStoreGenerateGenericHelper(masm, &fast_object_grow, &fast_double_grow,
                                  &slow, kDontCheckMap, kIncrementLength,
                                  value, key, receiver, receiver_map,
                                  elements_map, elements);
}


void StoreIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0    : value
  //  -- x1    : receiver
  //  -- x2    : name
  //  -- lr    : return address
  // -----------------------------------

  // Probe the stub cache.
  Code::Flags flags = Code::ComputeHandlerFlags(Code::STORE_IC);
  masm->isolate()->stub_cache()->GenerateProbe(
      masm, flags, x1, x2, x3, x4, x5, x6);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
}


void StoreIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0    : value
  //  -- x1    : receiver
  //  -- x2    : name
  //  -- lr    : return address
  // -----------------------------------

  __ Push(x1, x2, x0);

  // Tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kStoreIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void StoreIC::GenerateNormal(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- x0    : value
  //  -- x1    : receiver
  //  -- x2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;
  Register value = x0;
  Register receiver = x1;
  Register name = x2;
  Register dictionary = x3;

  GenerateNameDictionaryReceiverCheck(
      masm, receiver, dictionary, x4, x5, &miss);

  GenerateDictionaryStore(masm, &miss, dictionary, name, value, x4, x5);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->store_normal_hit(), 1, x4, x5);
  __ Ret();

  // Cache miss: Jump to runtime.
  __ Bind(&miss);
  __ IncrementCounter(counters->store_normal_miss(), 1, x4, x5);
  GenerateMiss(masm);
}


void StoreIC::GenerateRuntimeSetProperty(MacroAssembler* masm,
                                  StrictModeFlag strict_mode) {
  ASM_LOCATION("StoreIC::GenerateRuntimeSetProperty");
  // ----------- S t a t e -------------
  //  -- x0    : value
  //  -- x1    : receiver
  //  -- x2    : name
  //  -- lr    : return address
  // -----------------------------------

  __ Push(x1, x2, x0);

  __ Mov(x11, Operand(Smi::FromInt(NONE)));  // PropertyAttributes
  __ Mov(x10, Operand(Smi::FromInt(strict_mode)));
  __ Push(x11, x10);

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty, 5, 1);
}


void StoreIC::GenerateSlow(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- x0     : value
  //  -- x1     : receiver
  //  -- x2     : name
  //  -- lr     : return address
  // -----------------------------------

  // Push receiver, name and value for runtime call.
  __ Push(x1, x2, x0);

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  ExternalReference ref =
      ExternalReference(IC_Utility(kStoreIC_Slow), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


Condition CompareIC::ComputeCondition(Token::Value op) {
  switch (op) {
    case Token::EQ_STRICT:
    case Token::EQ:
      return eq;
    case Token::LT:
      return lt;
    case Token::GT:
      return gt;
    case Token::LTE:
      return le;
    case Token::GTE:
      return ge;
    default:
      UNREACHABLE();
      return al;
  }
}


bool CompareIC::HasInlinedSmiCode(Address address) {
  // The address of the instruction following the call.
  Address info_address =
      Assembler::return_address_from_call_start(address);

  InstructionSequence* patch_info = InstructionSequence::At(info_address);
  return patch_info->IsInlineData();
}


// Activate a SMI fast-path by patching the instructions generated by
// JumpPatchSite::EmitJumpIf(Not)Smi(), using the information encoded by
// JumpPatchSite::EmitPatchInfo().
void PatchInlinedSmiCode(Address address, InlinedSmiCheck check) {
  // The patch information is encoded in the instruction stream using
  // instructions which have no side effects, so we can safely execute them.
  // The patch information is encoded directly after the call to the helper
  // function which is requesting this patch operation.
  Address info_address =
      Assembler::return_address_from_call_start(address);
  InlineSmiCheckInfo info(info_address);

  // Check and decode the patch information instruction.
  if (!info.HasSmiCheck()) {
    return;
  }

  if (FLAG_trace_ic) {
    PrintF("[  Patching ic at %p, marker=%p, SMI check=%p\n",
           address, info_address, reinterpret_cast<void*>(info.SmiCheck()));
  }

  // Patch and activate code generated by JumpPatchSite::EmitJumpIfNotSmi()
  // and JumpPatchSite::EmitJumpIfSmi().
  // Changing
  //   tb(n)z xzr, #0, <target>
  // to
  //   tb(!n)z test_reg, #0, <target>
  Instruction* to_patch = info.SmiCheck();
  PatchingAssembler patcher(to_patch, 1);
  ASSERT(to_patch->IsTestBranch());
  ASSERT(to_patch->ImmTestBranchBit5() == 0);
  ASSERT(to_patch->ImmTestBranchBit40() == 0);

  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagMask == 1);

  int branch_imm = to_patch->ImmTestBranch();
  Register smi_reg;
  if (check == ENABLE_INLINED_SMI_CHECK) {
    ASSERT(to_patch->Rt() == xzr.code());
    smi_reg = info.SmiRegister();
  } else {
    ASSERT(check == DISABLE_INLINED_SMI_CHECK);
    ASSERT(to_patch->Rt() != xzr.code());
    smi_reg = xzr;
  }

  if (to_patch->Mask(TestBranchMask) == TBZ) {
    // This is JumpIfNotSmi(smi_reg, branch_imm).
    patcher.tbnz(smi_reg, 0, branch_imm);
  } else {
    ASSERT(to_patch->Mask(TestBranchMask) == TBNZ);
    // This is JumpIfSmi(smi_reg, branch_imm).
    patcher.tbz(smi_reg, 0, branch_imm);
  }
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_A64
