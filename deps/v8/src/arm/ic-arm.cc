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

#if defined(V8_TARGET_ARCH_ARM)

#include "assembler-arm.h"
#include "code-stubs.h"
#include "codegen-inl.h"
#include "disasm.h"
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
  __ cmp(type, Operand(JS_GLOBAL_OBJECT_TYPE));
  __ b(eq, global_object);
  __ cmp(type, Operand(JS_BUILTINS_OBJECT_TYPE));
  __ b(eq, global_object);
  __ cmp(type, Operand(JS_GLOBAL_PROXY_TYPE));
  __ b(eq, global_object);
}


// Generated code falls through if the receiver is a regular non-global
// JS object with slow properties and no interceptors.
static void GenerateStringDictionaryReceiverCheck(MacroAssembler* masm,
                                                  Register receiver,
                                                  Register elements,
                                                  Register t0,
                                                  Register t1,
                                                  Label* miss) {
  // Register usage:
  //   receiver: holds the receiver on entry and is unchanged.
  //   elements: holds the property dictionary on fall through.
  // Scratch registers:
  //   t0: used to holds the receiver map.
  //   t1: used to holds the receiver instance type, receiver bit mask and
  //       elements map.

  // Check that the receiver isn't a smi.
  __ tst(receiver, Operand(kSmiTagMask));
  __ b(eq, miss);

  // Check that the receiver is a valid JS object.
  __ CompareObjectType(receiver, t0, t1, FIRST_JS_OBJECT_TYPE);
  __ b(lt, miss);

  // If this assert fails, we have to check upper bound too.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);

  GenerateGlobalInstanceTypeCheck(masm, t1, miss);

  // Check that the global object does not require access checks.
  __ ldrb(t1, FieldMemOperand(t0, Map::kBitFieldOffset));
  __ tst(t1, Operand((1 << Map::kIsAccessCheckNeeded) |
                     (1 << Map::kHasNamedInterceptor)));
  __ b(ne, miss);

  __ ldr(elements, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ ldr(t1, FieldMemOperand(elements, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHashTableMapRootIndex);
  __ cmp(t1, ip);
  __ b(ne, miss);
}


// Probe the string dictionary in the |elements| register. Jump to the
// |done| label if a property with the given name is found. Jump to
// the |miss| label otherwise.
static void GenerateStringDictionaryProbes(MacroAssembler* masm,
                                           Label* miss,
                                           Label* done,
                                           Register elements,
                                           Register name,
                                           Register scratch1,
                                           Register scratch2) {
  // Assert that name contains a string.
  if (FLAG_debug_code) __ AbortIfNotString(name);

  // Compute the capacity mask.
  const int kCapacityOffset = StringDictionary::kHeaderSize +
      StringDictionary::kCapacityIndex * kPointerSize;
  __ ldr(scratch1, FieldMemOperand(elements, kCapacityOffset));
  __ mov(scratch1, Operand(scratch1, ASR, kSmiTagSize));  // convert smi to int
  __ sub(scratch1, scratch1, Operand(1));

  const int kElementsStartOffset = StringDictionary::kHeaderSize +
      StringDictionary::kElementsStartIndex * kPointerSize;

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  static const int kProbes = 4;
  for (int i = 0; i < kProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ ldr(scratch2, FieldMemOperand(name, String::kHashFieldOffset));
    if (i > 0) {
      // Add the probe offset (i + i * i) left shifted to avoid right shifting
      // the hash in a separate instruction. The value hash + i + i * i is right
      // shifted in the following and instruction.
      ASSERT(StringDictionary::GetProbeOffset(i) <
             1 << (32 - String::kHashFieldOffset));
      __ add(scratch2, scratch2, Operand(
          StringDictionary::GetProbeOffset(i) << String::kHashShift));
    }
    __ and_(scratch2, scratch1, Operand(scratch2, LSR, String::kHashShift));

    // Scale the index by multiplying by the element size.
    ASSERT(StringDictionary::kEntrySize == 3);
    // scratch2 = scratch2 * 3.
    __ add(scratch2, scratch2, Operand(scratch2, LSL, 1));

    // Check if the key is identical to the name.
    __ add(scratch2, elements, Operand(scratch2, LSL, 2));
    __ ldr(ip, FieldMemOperand(scratch2, kElementsStartOffset));
    __ cmp(name, Operand(ip));
    if (i != kProbes - 1) {
      __ b(eq, done);
    } else {
      __ b(ne, miss);
    }
  }
}


// Helper function used from LoadIC/CallIC GenerateNormal.
//
// elements: Property dictionary. It is not clobbered if a jump to the miss
//           label is done.
// name:     Property name. It is not clobbered if a jump to the miss label is
//           done
// result:   Register for the result. It is only updated if a jump to the miss
//           label is not done. Can be the same as elements or name clobbering
//           one of these in the case of not jumping to the miss label.
// The two scratch registers need to be different from elements, name and
// result.
// The generated code assumes that the receiver has slow properties,
// is not a global object and does not have interceptors.
static void GenerateDictionaryLoad(MacroAssembler* masm,
                                   Label* miss,
                                   Register elements,
                                   Register name,
                                   Register result,
                                   Register scratch1,
                                   Register scratch2) {
  // Main use of the scratch registers.
  // scratch1: Used as temporary and to hold the capacity of the property
  //           dictionary.
  // scratch2: Used as temporary.
  Label done;

  // Probe the dictionary.
  GenerateStringDictionaryProbes(masm,
                                 miss,
                                 &done,
                                 elements,
                                 name,
                                 scratch1,
                                 scratch2);

  // If probing finds an entry check that the value is a normal
  // property.
  __ bind(&done);  // scratch2 == elements + 4 * index
  const int kElementsStartOffset = StringDictionary::kHeaderSize +
      StringDictionary::kElementsStartIndex * kPointerSize;
  const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  __ ldr(scratch1, FieldMemOperand(scratch2, kDetailsOffset));
  __ tst(scratch1, Operand(PropertyDetails::TypeField::mask() << kSmiTagSize));
  __ b(ne, miss);

  // Get the value at the masked, scaled index and return.
  __ ldr(result,
         FieldMemOperand(scratch2, kElementsStartOffset + 1 * kPointerSize));
}


// Helper function used from StoreIC::GenerateNormal.
//
// elements: Property dictionary. It is not clobbered if a jump to the miss
//           label is done.
// name:     Property name. It is not clobbered if a jump to the miss label is
//           done
// value:    The value to store.
// The two scratch registers need to be different from elements, name and
// result.
// The generated code assumes that the receiver has slow properties,
// is not a global object and does not have interceptors.
static void GenerateDictionaryStore(MacroAssembler* masm,
                                    Label* miss,
                                    Register elements,
                                    Register name,
                                    Register value,
                                    Register scratch1,
                                    Register scratch2) {
  // Main use of the scratch registers.
  // scratch1: Used as temporary and to hold the capacity of the property
  //           dictionary.
  // scratch2: Used as temporary.
  Label done;

  // Probe the dictionary.
  GenerateStringDictionaryProbes(masm,
                                 miss,
                                 &done,
                                 elements,
                                 name,
                                 scratch1,
                                 scratch2);

  // If probing finds an entry in the dictionary check that the value
  // is a normal property that is not read only.
  __ bind(&done);  // scratch2 == elements + 4 * index
  const int kElementsStartOffset = StringDictionary::kHeaderSize +
      StringDictionary::kElementsStartIndex * kPointerSize;
  const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  const int kTypeAndReadOnlyMask
      = (PropertyDetails::TypeField::mask() |
         PropertyDetails::AttributesField::encode(READ_ONLY)) << kSmiTagSize;
  __ ldr(scratch1, FieldMemOperand(scratch2, kDetailsOffset));
  __ tst(scratch1, Operand(kTypeAndReadOnlyMask));
  __ b(ne, miss);

  // Store the value at the masked, scaled index and return.
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ add(scratch2, scratch2, Operand(kValueOffset - kHeapObjectTag));
  __ str(value, MemOperand(scratch2));

  // Update the write barrier. Make sure not to clobber the value.
  __ mov(scratch1, value);
  __ RecordWrite(elements, scratch2, scratch1);
}


static void GenerateNumberDictionaryLoad(MacroAssembler* masm,
                                         Label* miss,
                                         Register elements,
                                         Register key,
                                         Register result,
                                         Register t0,
                                         Register t1,
                                         Register t2) {
  // Register use:
  //
  // elements - holds the slow-case elements of the receiver on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // key      - holds the smi key on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // result   - holds the result on exit if the load succeeded.
  //            Allowed to be the same as 'key' or 'result'.
  //            Unchanged on bailout so 'key' or 'result' can be used
  //            in further computation.
  //
  // Scratch registers:
  //
  // t0 - holds the untagged key on entry and holds the hash once computed.
  //
  // t1 - used to hold the capacity mask of the dictionary
  //
  // t2 - used for the index into the dictionary.
  Label done;

  // Compute the hash code from the untagged key.  This must be kept in sync
  // with ComputeIntegerHash in utils.h.
  //
  // hash = ~hash + (hash << 15);
  __ mvn(t1, Operand(t0));
  __ add(t0, t1, Operand(t0, LSL, 15));
  // hash = hash ^ (hash >> 12);
  __ eor(t0, t0, Operand(t0, LSR, 12));
  // hash = hash + (hash << 2);
  __ add(t0, t0, Operand(t0, LSL, 2));
  // hash = hash ^ (hash >> 4);
  __ eor(t0, t0, Operand(t0, LSR, 4));
  // hash = hash * 2057;
  __ mov(t1, Operand(2057));
  __ mul(t0, t0, t1);
  // hash = hash ^ (hash >> 16);
  __ eor(t0, t0, Operand(t0, LSR, 16));

  // Compute the capacity mask.
  __ ldr(t1, FieldMemOperand(elements, NumberDictionary::kCapacityOffset));
  __ mov(t1, Operand(t1, ASR, kSmiTagSize));  // convert smi to int
  __ sub(t1, t1, Operand(1));

  // Generate an unrolled loop that performs a few probes before giving up.
  static const int kProbes = 4;
  for (int i = 0; i < kProbes; i++) {
    // Use t2 for index calculations and keep the hash intact in t0.
    __ mov(t2, t0);
    // Compute the masked index: (hash + i + i * i) & mask.
    if (i > 0) {
      __ add(t2, t2, Operand(NumberDictionary::GetProbeOffset(i)));
    }
    __ and_(t2, t2, Operand(t1));

    // Scale the index by multiplying by the element size.
    ASSERT(NumberDictionary::kEntrySize == 3);
    __ add(t2, t2, Operand(t2, LSL, 1));  // t2 = t2 * 3

    // Check if the key is identical to the name.
    __ add(t2, elements, Operand(t2, LSL, kPointerSizeLog2));
    __ ldr(ip, FieldMemOperand(t2, NumberDictionary::kElementsStartOffset));
    __ cmp(key, Operand(ip));
    if (i != kProbes - 1) {
      __ b(eq, &done);
    } else {
      __ b(ne, miss);
    }
  }

  __ bind(&done);
  // Check that the value is a normal property.
  // t2: elements + (index * kPointerSize)
  const int kDetailsOffset =
      NumberDictionary::kElementsStartOffset + 2 * kPointerSize;
  __ ldr(t1, FieldMemOperand(t2, kDetailsOffset));
  __ tst(t1, Operand(Smi::FromInt(PropertyDetails::TypeField::mask())));
  __ b(ne, miss);

  // Get the value at the masked, scaled index and return.
  const int kValueOffset =
      NumberDictionary::kElementsStartOffset + kPointerSize;
  __ ldr(result, FieldMemOperand(t2, kValueOffset));
}


void LoadIC::GenerateArrayLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- r0    : receiver
  //  -- sp[0] : receiver
  // -----------------------------------
  Label miss;

  StubCompiler::GenerateLoadArrayLength(masm, r0, r3, &miss);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateStringLength(MacroAssembler* masm, bool support_wrappers) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- r0    : receiver
  //  -- sp[0] : receiver
  // -----------------------------------
  Label miss;

  StubCompiler::GenerateLoadStringLength(masm, r0, r1, r3, &miss,
                                         support_wrappers);
  // Cache miss: Jump to runtime.
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateFunctionPrototype(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- r0    : receiver
  //  -- sp[0] : receiver
  // -----------------------------------
  Label miss;

  StubCompiler::GenerateLoadFunctionPrototype(masm, r0, r1, r3, &miss);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


// Checks the receiver for special cases (value type, slow case bits).
// Falls through for regular JS object.
static void GenerateKeyedLoadReceiverCheck(MacroAssembler* masm,
                                           Register receiver,
                                           Register map,
                                           Register scratch,
                                           int interceptor_bit,
                                           Label* slow) {
  // Check that the object isn't a smi.
  __ JumpIfSmi(receiver, slow);
  // Get the map of the receiver.
  __ ldr(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  // Check bit field.
  __ ldrb(scratch, FieldMemOperand(map, Map::kBitFieldOffset));
  __ tst(scratch,
         Operand((1 << Map::kIsAccessCheckNeeded) | (1 << interceptor_bit)));
  __ b(ne, slow);
  // Check that the object is some kind of JS object EXCEPT JS Value type.
  // In the case that the object is a value-wrapper object,
  // we enter the runtime system to make sure that indexing into string
  // objects work as intended.
  ASSERT(JS_OBJECT_TYPE > JS_VALUE_TYPE);
  __ ldrb(scratch, FieldMemOperand(map, Map::kInstanceTypeOffset));
  __ cmp(scratch, Operand(JS_OBJECT_TYPE));
  __ b(lt, slow);
}


// Loads an indexed element from a fast case array.
// If not_fast_array is NULL, doesn't perform the elements map check.
static void GenerateFastArrayLoad(MacroAssembler* masm,
                                  Register receiver,
                                  Register key,
                                  Register elements,
                                  Register scratch1,
                                  Register scratch2,
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
  // scratch1 - used to hold elements map and elements length.
  //            Holds the elements map if not_fast_array branch is taken.
  //
  // scratch2 - used to hold the loaded value.

  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  if (not_fast_array != NULL) {
    // Check that the object is in fast mode and writable.
    __ ldr(scratch1, FieldMemOperand(elements, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kFixedArrayMapRootIndex);
    __ cmp(scratch1, ip);
    __ b(ne, not_fast_array);
  } else {
    __ AssertFastElements(elements);
  }
  // Check that the key (index) is within bounds.
  __ ldr(scratch1, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ cmp(key, Operand(scratch1));
  __ b(hs, out_of_range);
  // Fast case: Do the load.
  __ add(scratch1, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  // The key is a smi.
  ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
  __ ldr(scratch2,
         MemOperand(scratch1, key, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ cmp(scratch2, ip);
  // In case the loaded value is the_hole we have to consult GetProperty
  // to ensure the prototype chain is searched.
  __ b(eq, out_of_range);
  __ mov(result, scratch2);
}


// Checks whether a key is an array index string or a symbol string.
// Falls through if a key is a symbol.
static void GenerateKeyStringCheck(MacroAssembler* masm,
                                   Register key,
                                   Register map,
                                   Register hash,
                                   Label* index_string,
                                   Label* not_symbol) {
  // The key is not a smi.
  // Is it a string?
  __ CompareObjectType(key, map, hash, FIRST_NONSTRING_TYPE);
  __ b(ge, not_symbol);

  // Is the string an array index, with cached numeric value?
  __ ldr(hash, FieldMemOperand(key, String::kHashFieldOffset));
  __ tst(hash, Operand(String::kContainsCachedArrayIndexMask));
  __ b(eq, index_string);

  // Is the string a symbol?
  // map: key map
  __ ldrb(hash, FieldMemOperand(map, Map::kInstanceTypeOffset));
  ASSERT(kSymbolTag != 0);
  __ tst(hash, Operand(kIsSymbolMask));
  __ b(eq, not_symbol);
}


// Defined in ic.cc.
Object* CallIC_Miss(Arguments args);

// The generated code does not accept smi keys.
// The generated code falls through if both probes miss.
static void GenerateMonomorphicCacheProbe(MacroAssembler* masm,
                                          int argc,
                                          Code::Kind kind) {
  // ----------- S t a t e -------------
  //  -- r1    : receiver
  //  -- r2    : name
  // -----------------------------------
  Label number, non_number, non_string, boolean, probe, miss;

  // Probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(kind,
                                         NOT_IN_LOOP,
                                         MONOMORPHIC,
                                         Code::kNoExtraICState,
                                         NORMAL,
                                         argc);
  StubCache::GenerateProbe(masm, flags, r1, r2, r3, r4, r5);

  // If the stub cache probing failed, the receiver might be a value.
  // For value objects, we use the map of the prototype objects for
  // the corresponding JSValue for the cache and that is what we need
  // to probe.
  //
  // Check for number.
  __ tst(r1, Operand(kSmiTagMask));
  __ b(eq, &number);
  __ CompareObjectType(r1, r3, r3, HEAP_NUMBER_TYPE);
  __ b(ne, &non_number);
  __ bind(&number);
  StubCompiler::GenerateLoadGlobalFunctionPrototype(
      masm, Context::NUMBER_FUNCTION_INDEX, r1);
  __ b(&probe);

  // Check for string.
  __ bind(&non_number);
  __ cmp(r3, Operand(FIRST_NONSTRING_TYPE));
  __ b(hs, &non_string);
  StubCompiler::GenerateLoadGlobalFunctionPrototype(
      masm, Context::STRING_FUNCTION_INDEX, r1);
  __ b(&probe);

  // Check for boolean.
  __ bind(&non_string);
  __ LoadRoot(ip, Heap::kTrueValueRootIndex);
  __ cmp(r1, ip);
  __ b(eq, &boolean);
  __ LoadRoot(ip, Heap::kFalseValueRootIndex);
  __ cmp(r1, ip);
  __ b(ne, &miss);
  __ bind(&boolean);
  StubCompiler::GenerateLoadGlobalFunctionPrototype(
      masm, Context::BOOLEAN_FUNCTION_INDEX, r1);

  // Probe the stub cache for the value object.
  __ bind(&probe);
  StubCache::GenerateProbe(masm, flags, r1, r2, r3, r4, r5);

  __ bind(&miss);
}


static void GenerateFunctionTailCall(MacroAssembler* masm,
                                     int argc,
                                     Label* miss,
                                     Register scratch) {
  // r1: function

  // Check that the value isn't a smi.
  __ tst(r1, Operand(kSmiTagMask));
  __ b(eq, miss);

  // Check that the value is a JSFunction.
  __ CompareObjectType(r1, scratch, scratch, JS_FUNCTION_TYPE);
  __ b(ne, miss);

  // Invoke the function.
  ParameterCount actual(argc);
  __ InvokeFunction(r1, actual, JUMP_FUNCTION);
}


static void GenerateCallNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  // Get the receiver of the function from the stack into r1.
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));

  GenerateStringDictionaryReceiverCheck(masm, r1, r0, r3, r4, &miss);

  // r0: elements
  // Search the dictionary - put result in register r1.
  GenerateDictionaryLoad(masm, &miss, r0, r2, r1, r3, r4);

  GenerateFunctionTailCall(masm, argc, &miss, r4);

  __ bind(&miss);
}


static void GenerateCallMiss(MacroAssembler* masm, int argc, IC::UtilityId id) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  if (id == IC::kCallIC_Miss) {
    __ IncrementCounter(&Counters::call_miss, 1, r3, r4);
  } else {
    __ IncrementCounter(&Counters::keyed_call_miss, 1, r3, r4);
  }

  // Get the receiver of the function from the stack.
  __ ldr(r3, MemOperand(sp, argc * kPointerSize));

  __ EnterInternalFrame();

  // Push the receiver and the name of the function.
  __ Push(r3, r2);

  // Call the entry.
  __ mov(r0, Operand(2));
  __ mov(r1, Operand(ExternalReference(IC_Utility(id))));

  CEntryStub stub(1);
  __ CallStub(&stub);

  // Move result to r1 and leave the internal frame.
  __ mov(r1, Operand(r0));
  __ LeaveInternalFrame();

  // Check if the receiver is a global object of some sort.
  // This can happen only for regular CallIC but not KeyedCallIC.
  if (id == IC::kCallIC_Miss) {
    Label invoke, global;
    __ ldr(r2, MemOperand(sp, argc * kPointerSize));  // receiver
    __ tst(r2, Operand(kSmiTagMask));
    __ b(eq, &invoke);
    __ CompareObjectType(r2, r3, r3, JS_GLOBAL_OBJECT_TYPE);
    __ b(eq, &global);
    __ cmp(r3, Operand(JS_BUILTINS_OBJECT_TYPE));
    __ b(ne, &invoke);

    // Patch the receiver on the stack.
    __ bind(&global);
    __ ldr(r2, FieldMemOperand(r2, GlobalObject::kGlobalReceiverOffset));
    __ str(r2, MemOperand(sp, argc * kPointerSize));
    __ bind(&invoke);
  }

  // Invoke the function.
  ParameterCount actual(argc);
  __ InvokeFunction(r1, actual, JUMP_FUNCTION);
}


void CallIC::GenerateMiss(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  GenerateCallMiss(masm, argc, IC::kCallIC_Miss);
}


void CallIC::GenerateMegamorphic(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  // Get the receiver of the function from the stack into r1.
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));
  GenerateMonomorphicCacheProbe(masm, argc, Code::CALL_IC);
  GenerateMiss(masm, argc);
}


void CallIC::GenerateNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  GenerateCallNormal(masm, argc);
  GenerateMiss(masm, argc);
}


void KeyedCallIC::GenerateMiss(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  GenerateCallMiss(masm, argc, IC::kKeyedCallIC_Miss);
}


void KeyedCallIC::GenerateMegamorphic(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  // Get the receiver of the function from the stack into r1.
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));

  Label do_call, slow_call, slow_load, slow_reload_receiver;
  Label check_number_dictionary, check_string, lookup_monomorphic_cache;
  Label index_smi, index_string;

  // Check that the key is a smi.
  __ JumpIfNotSmi(r2, &check_string);
  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from below
  // where a numeric string is converted to a smi.

  GenerateKeyedLoadReceiverCheck(
      masm, r1, r0, r3, Map::kHasIndexedInterceptor, &slow_call);

  GenerateFastArrayLoad(
      masm, r1, r2, r4, r3, r0, r1, &check_number_dictionary, &slow_load);
  __ IncrementCounter(&Counters::keyed_call_generic_smi_fast, 1, r0, r3);

  __ bind(&do_call);
  // receiver in r1 is not used after this point.
  // r2: key
  // r1: function
  GenerateFunctionTailCall(masm, argc, &slow_call, r0);

  __ bind(&check_number_dictionary);
  // r2: key
  // r3: elements map
  // r4: elements
  // Check whether the elements is a number dictionary.
  __ LoadRoot(ip, Heap::kHashTableMapRootIndex);
  __ cmp(r3, ip);
  __ b(ne, &slow_load);
  __ mov(r0, Operand(r2, ASR, kSmiTagSize));
  // r0: untagged index
  GenerateNumberDictionaryLoad(masm, &slow_load, r4, r2, r1, r0, r3, r5);
  __ IncrementCounter(&Counters::keyed_call_generic_smi_dict, 1, r0, r3);
  __ jmp(&do_call);

  __ bind(&slow_load);
  // This branch is taken when calling KeyedCallIC_Miss is neither required
  // nor beneficial.
  __ IncrementCounter(&Counters::keyed_call_generic_slow_load, 1, r0, r3);
  __ EnterInternalFrame();
  __ push(r2);  // save the key
  __ Push(r1, r2);  // pass the receiver and the key
  __ CallRuntime(Runtime::kKeyedGetProperty, 2);
  __ pop(r2);  // restore the key
  __ LeaveInternalFrame();
  __ mov(r1, r0);
  __ jmp(&do_call);

  __ bind(&check_string);
  GenerateKeyStringCheck(masm, r2, r0, r3, &index_string, &slow_call);

  // The key is known to be a symbol.
  // If the receiver is a regular JS object with slow properties then do
  // a quick inline probe of the receiver's dictionary.
  // Otherwise do the monomorphic cache probe.
  GenerateKeyedLoadReceiverCheck(
      masm, r1, r0, r3, Map::kHasNamedInterceptor, &lookup_monomorphic_cache);

  __ ldr(r0, FieldMemOperand(r1, JSObject::kPropertiesOffset));
  __ ldr(r3, FieldMemOperand(r0, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHashTableMapRootIndex);
  __ cmp(r3, ip);
  __ b(ne, &lookup_monomorphic_cache);

  GenerateDictionaryLoad(masm, &slow_load, r0, r2, r1, r3, r4);
  __ IncrementCounter(&Counters::keyed_call_generic_lookup_dict, 1, r0, r3);
  __ jmp(&do_call);

  __ bind(&lookup_monomorphic_cache);
  __ IncrementCounter(&Counters::keyed_call_generic_lookup_cache, 1, r0, r3);
  GenerateMonomorphicCacheProbe(masm, argc, Code::KEYED_CALL_IC);
  // Fall through on miss.

  __ bind(&slow_call);
  // This branch is taken if:
  // - the receiver requires boxing or access check,
  // - the key is neither smi nor symbol,
  // - the value loaded is not a function,
  // - there is hope that the runtime will create a monomorphic call stub
  //   that will get fetched next time.
  __ IncrementCounter(&Counters::keyed_call_generic_slow, 1, r0, r3);
  GenerateMiss(masm, argc);

  __ bind(&index_string);
  __ IndexFromHash(r3, r2);
  // Now jump to the place where smi keys are handled.
  __ jmp(&index_smi);
}


void KeyedCallIC::GenerateNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  // Check if the name is a string.
  Label miss;
  __ tst(r2, Operand(kSmiTagMask));
  __ b(eq, &miss);
  __ IsObjectJSStringType(r2, r0, &miss);

  GenerateCallNormal(masm, argc);
  __ bind(&miss);
  GenerateMiss(masm, argc);
}


// Defined in ic.cc.
Object* LoadIC_Miss(Arguments args);

void LoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- r0    : receiver
  //  -- sp[0] : receiver
  // -----------------------------------

  // Probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(Code::LOAD_IC,
                                         NOT_IN_LOOP,
                                         MONOMORPHIC);
  StubCache::GenerateProbe(masm, flags, r0, r2, r3, r4, r5);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
}


void LoadIC::GenerateNormal(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- r0    : receiver
  //  -- sp[0] : receiver
  // -----------------------------------
  Label miss;

  GenerateStringDictionaryReceiverCheck(masm, r0, r1, r3, r4, &miss);

  // r1: elements
  GenerateDictionaryLoad(masm, &miss, r1, r2, r0, r3, r4);
  __ Ret();

  // Cache miss: Jump to runtime.
  __ bind(&miss);
  GenerateMiss(masm);
}


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- r0    : receiver
  //  -- sp[0] : receiver
  // -----------------------------------

  __ IncrementCounter(&Counters::load_miss, 1, r3, r4);

  __ mov(r3, r0);
  __ Push(r3, r2);

  // Perform tail call to the entry.
  ExternalReference ref = ExternalReference(IC_Utility(kLoadIC_Miss));
  __ TailCallExternalReference(ref, 2, 1);
}

// Returns the code marker, or the 0 if the code is not marked.
static inline int InlinedICSiteMarker(Address address,
                                      Address* inline_end_address) {
  if (V8::UseCrankshaft()) return false;

  // If the instruction after the call site is not the pseudo instruction nop1
  // then this is not related to an inlined in-object property load. The nop1
  // instruction is located just after the call to the IC in the deferred code
  // handling the miss in the inlined code. After the nop1 instruction there is
  // a branch instruction for jumping back from the deferred code.
  Address address_after_call = address + Assembler::kCallTargetAddressOffset;
  Instr instr_after_call = Assembler::instr_at(address_after_call);
  int code_marker = MacroAssembler::GetCodeMarker(instr_after_call);

  // A negative result means the code is not marked.
  if (code_marker <= 0) return 0;

  Address address_after_nop = address_after_call + Assembler::kInstrSize;
  Instr instr_after_nop = Assembler::instr_at(address_after_nop);
  // There may be some reg-reg move and frame merging code to skip over before
  // the branch back from the DeferredReferenceGetKeyedValue code to the inlined
  // code.
  while (!Assembler::IsBranch(instr_after_nop)) {
    address_after_nop += Assembler::kInstrSize;
    instr_after_nop = Assembler::instr_at(address_after_nop);
  }

  // Find the end of the inlined code for handling the load.
  int b_offset =
      Assembler::GetBranchOffset(instr_after_nop) + Assembler::kPcLoadDelta;
  ASSERT(b_offset < 0);  // Jumping back from deferred code.
  *inline_end_address = address_after_nop + b_offset;

  return code_marker;
}


bool LoadIC::PatchInlinedLoad(Address address, Object* map, int offset) {
  if (V8::UseCrankshaft()) return false;

  // Find the end of the inlined code for handling the load if this is an
  // inlined IC call site.
  Address inline_end_address;
  if (InlinedICSiteMarker(address, &inline_end_address)
      != Assembler::PROPERTY_ACCESS_INLINED) {
    return false;
  }

  // Patch the offset of the property load instruction (ldr r0, [r1, #+XXX]).
  // The immediate must be representable in 12 bits.
  ASSERT((JSObject::kMaxInstanceSize - JSObject::kHeaderSize) < (1 << 12));
  Address ldr_property_instr_address =
      inline_end_address - Assembler::kInstrSize;
  ASSERT(Assembler::IsLdrRegisterImmediate(
      Assembler::instr_at(ldr_property_instr_address)));
  Instr ldr_property_instr = Assembler::instr_at(ldr_property_instr_address);
  ldr_property_instr = Assembler::SetLdrRegisterImmediateOffset(
      ldr_property_instr, offset - kHeapObjectTag);
  Assembler::instr_at_put(ldr_property_instr_address, ldr_property_instr);

  // Indicate that code has changed.
  CPU::FlushICache(ldr_property_instr_address, 1 * Assembler::kInstrSize);

  // Patch the map check.
  // For PROPERTY_ACCESS_INLINED, the load map instruction is generated
  // 4 instructions before the end of the inlined code.
  // See codgen-arm.cc CodeGenerator::EmitNamedLoad.
  int ldr_map_offset = -4;
  Address ldr_map_instr_address =
      inline_end_address + ldr_map_offset * Assembler::kInstrSize;
  Assembler::set_target_address_at(ldr_map_instr_address,
                                   reinterpret_cast<Address>(map));
  return true;
}


bool LoadIC::PatchInlinedContextualLoad(Address address,
                                        Object* map,
                                        Object* cell,
                                        bool is_dont_delete) {
  // Find the end of the inlined code for handling the contextual load if
  // this is inlined IC call site.
  Address inline_end_address;
  int marker = InlinedICSiteMarker(address, &inline_end_address);
  if (!((marker == Assembler::PROPERTY_ACCESS_INLINED_CONTEXT) ||
        (marker == Assembler::PROPERTY_ACCESS_INLINED_CONTEXT_DONT_DELETE))) {
    return false;
  }
  // On ARM we don't rely on the is_dont_delete argument as the hint is already
  // embedded in the code marker.
  bool marker_is_dont_delete =
      marker == Assembler::PROPERTY_ACCESS_INLINED_CONTEXT_DONT_DELETE;

  // These are the offsets from the end of the inlined code.
  // See codgen-arm.cc CodeGenerator::EmitNamedLoad.
  int ldr_map_offset = marker_is_dont_delete ? -5: -8;
  int ldr_cell_offset = marker_is_dont_delete ? -2: -5;
  if (FLAG_debug_code && marker_is_dont_delete) {
    // Three extra instructions were generated to check for the_hole_value.
    ldr_map_offset -= 3;
    ldr_cell_offset -= 3;
  }
  Address ldr_map_instr_address =
      inline_end_address + ldr_map_offset * Assembler::kInstrSize;
  Address ldr_cell_instr_address =
      inline_end_address + ldr_cell_offset * Assembler::kInstrSize;

  // Patch the map check.
  Assembler::set_target_address_at(ldr_map_instr_address,
                                   reinterpret_cast<Address>(map));
  // Patch the cell address.
  Assembler::set_target_address_at(ldr_cell_instr_address,
                                   reinterpret_cast<Address>(cell));

  return true;
}


bool StoreIC::PatchInlinedStore(Address address, Object* map, int offset) {
  if (V8::UseCrankshaft()) return false;

  // Find the end of the inlined code for the store if there is an
  // inlined version of the store.
  Address inline_end_address;
  if (InlinedICSiteMarker(address, &inline_end_address)
      != Assembler::PROPERTY_ACCESS_INLINED) {
    return false;
  }

  // Compute the address of the map load instruction.
  Address ldr_map_instr_address =
      inline_end_address -
      (CodeGenerator::GetInlinedNamedStoreInstructionsAfterPatch() *
       Assembler::kInstrSize);

  // Update the offsets if initializing the inlined store. No reason
  // to update the offsets when clearing the inlined version because
  // it will bail out in the map check.
  if (map != Heap::null_value()) {
    // Patch the offset in the actual store instruction.
    Address str_property_instr_address =
        ldr_map_instr_address + 3 * Assembler::kInstrSize;
    Instr str_property_instr = Assembler::instr_at(str_property_instr_address);
    ASSERT(Assembler::IsStrRegisterImmediate(str_property_instr));
    str_property_instr = Assembler::SetStrRegisterImmediateOffset(
        str_property_instr, offset - kHeapObjectTag);
    Assembler::instr_at_put(str_property_instr_address, str_property_instr);

    // Patch the offset in the add instruction that is part of the
    // write barrier.
    Address add_offset_instr_address =
        str_property_instr_address + Assembler::kInstrSize;
    Instr add_offset_instr = Assembler::instr_at(add_offset_instr_address);
    ASSERT(Assembler::IsAddRegisterImmediate(add_offset_instr));
    add_offset_instr = Assembler::SetAddRegisterImmediateOffset(
        add_offset_instr, offset - kHeapObjectTag);
    Assembler::instr_at_put(add_offset_instr_address, add_offset_instr);

    // Indicate that code has changed.
    CPU::FlushICache(str_property_instr_address, 2 * Assembler::kInstrSize);
  }

  // Patch the map check.
  Assembler::set_target_address_at(ldr_map_instr_address,
                                   reinterpret_cast<Address>(map));

  return true;
}


bool KeyedLoadIC::PatchInlinedLoad(Address address, Object* map) {
  if (V8::UseCrankshaft()) return false;

  Address inline_end_address;
  if (InlinedICSiteMarker(address, &inline_end_address)
      != Assembler::PROPERTY_ACCESS_INLINED) {
    return false;
  }

  // Patch the map check.
  Address ldr_map_instr_address =
      inline_end_address -
      (CodeGenerator::GetInlinedKeyedLoadInstructionsAfterPatch() *
      Assembler::kInstrSize);
  Assembler::set_target_address_at(ldr_map_instr_address,
                                   reinterpret_cast<Address>(map));
  return true;
}


bool KeyedStoreIC::PatchInlinedStore(Address address, Object* map) {
  if (V8::UseCrankshaft()) return false;

  // Find the end of the inlined code for handling the store if this is an
  // inlined IC call site.
  Address inline_end_address;
  if (InlinedICSiteMarker(address, &inline_end_address)
      != Assembler::PROPERTY_ACCESS_INLINED) {
    return false;
  }

  // Patch the map check.
  Address ldr_map_instr_address =
      inline_end_address -
      (CodeGenerator::kInlinedKeyedStoreInstructionsAfterPatch *
      Assembler::kInstrSize);
  Assembler::set_target_address_at(ldr_map_instr_address,
                                   reinterpret_cast<Address>(map));
  return true;
}


Object* KeyedLoadIC_Miss(Arguments args);


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------

  __ IncrementCounter(&Counters::keyed_load_miss, 1, r3, r4);

  __ Push(r1, r0);

  ExternalReference ref = ExternalReference(IC_Utility(kKeyedLoadIC_Miss));
  __ TailCallExternalReference(ref, 2, 1);
}


void KeyedLoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------

  __ Push(r1, r0);

  __ TailCallRuntime(Runtime::kKeyedGetProperty, 2, 1);
}


void KeyedLoadIC::GenerateGeneric(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------
  Label slow, check_string, index_smi, index_string, property_array_property;
  Label check_pixel_array, probe_dictionary, check_number_dictionary;

  Register key = r0;
  Register receiver = r1;

  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &check_string);
  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from below
  // where a numeric string is converted to a smi.

  GenerateKeyedLoadReceiverCheck(
      masm, receiver, r2, r3, Map::kHasIndexedInterceptor, &slow);

  // Check the "has fast elements" bit in the receiver's map which is
  // now in r2.
  __ ldrb(r3, FieldMemOperand(r2, Map::kBitField2Offset));
  __ tst(r3, Operand(1 << Map::kHasFastElements));
  __ b(eq, &check_pixel_array);

  GenerateFastArrayLoad(
      masm, receiver, key, r4, r3, r2, r0, NULL, &slow);
  __ IncrementCounter(&Counters::keyed_load_generic_smi, 1, r2, r3);
  __ Ret();

  // Check whether the elements is a pixel array.
  // r0: key
  // r1: receiver
  __ bind(&check_pixel_array);

  GenerateFastPixelArrayLoad(masm,
                             r1,
                             r0,
                             r3,
                             r4,
                             r2,
                             r5,
                             r0,
                             &check_number_dictionary,
                             NULL,
                             &slow);

  __ bind(&check_number_dictionary);
  // Check whether the elements is a number dictionary.
  // r0: key
  // r3: elements map
  // r4: elements
  __ LoadRoot(ip, Heap::kHashTableMapRootIndex);
  __ cmp(r3, ip);
  __ b(ne, &slow);
  __ mov(r2, Operand(r0, ASR, kSmiTagSize));
  GenerateNumberDictionaryLoad(masm, &slow, r4, r0, r0, r2, r3, r5);
  __ Ret();

  // Slow case, key and receiver still in r0 and r1.
  __ bind(&slow);
  __ IncrementCounter(&Counters::keyed_load_generic_slow, 1, r2, r3);
  GenerateRuntimeGetProperty(masm);

  __ bind(&check_string);
  GenerateKeyStringCheck(masm, key, r2, r3, &index_string, &slow);

  GenerateKeyedLoadReceiverCheck(
      masm, receiver, r2, r3, Map::kHasNamedInterceptor, &slow);

  // If the receiver is a fast-case object, check the keyed lookup
  // cache. Otherwise probe the dictionary.
  __ ldr(r3, FieldMemOperand(r1, JSObject::kPropertiesOffset));
  __ ldr(r4, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHashTableMapRootIndex);
  __ cmp(r4, ip);
  __ b(eq, &probe_dictionary);

  // Load the map of the receiver, compute the keyed lookup cache hash
  // based on 32 bits of the map pointer and the string hash.
  __ ldr(r2, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ mov(r3, Operand(r2, ASR, KeyedLookupCache::kMapHashShift));
  __ ldr(r4, FieldMemOperand(r0, String::kHashFieldOffset));
  __ eor(r3, r3, Operand(r4, ASR, String::kHashShift));
  __ And(r3, r3, Operand(KeyedLookupCache::kCapacityMask));

  // Load the key (consisting of map and symbol) from the cache and
  // check for match.
  ExternalReference cache_keys = ExternalReference::keyed_lookup_cache_keys();
  __ mov(r4, Operand(cache_keys));
  __ add(r4, r4, Operand(r3, LSL, kPointerSizeLog2 + 1));
  __ ldr(r5, MemOperand(r4, kPointerSize, PostIndex));  // Move r4 to symbol.
  __ cmp(r2, r5);
  __ b(ne, &slow);
  __ ldr(r5, MemOperand(r4));
  __ cmp(r0, r5);
  __ b(ne, &slow);

  // Get field offset.
  // r0     : key
  // r1     : receiver
  // r2     : receiver's map
  // r3     : lookup cache index
  ExternalReference cache_field_offsets
      = ExternalReference::keyed_lookup_cache_field_offsets();
  __ mov(r4, Operand(cache_field_offsets));
  __ ldr(r5, MemOperand(r4, r3, LSL, kPointerSizeLog2));
  __ ldrb(r6, FieldMemOperand(r2, Map::kInObjectPropertiesOffset));
  __ sub(r5, r5, r6, SetCC);
  __ b(ge, &property_array_property);

  // Load in-object property.
  __ ldrb(r6, FieldMemOperand(r2, Map::kInstanceSizeOffset));
  __ add(r6, r6, r5);  // Index from start of object.
  __ sub(r1, r1, Operand(kHeapObjectTag));  // Remove the heap tag.
  __ ldr(r0, MemOperand(r1, r6, LSL, kPointerSizeLog2));
  __ IncrementCounter(&Counters::keyed_load_generic_lookup_cache, 1, r2, r3);
  __ Ret();

  // Load property array property.
  __ bind(&property_array_property);
  __ ldr(r1, FieldMemOperand(r1, JSObject::kPropertiesOffset));
  __ add(r1, r1, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ ldr(r0, MemOperand(r1, r5, LSL, kPointerSizeLog2));
  __ IncrementCounter(&Counters::keyed_load_generic_lookup_cache, 1, r2, r3);
  __ Ret();

  // Do a quick inline probe of the receiver's dictionary, if it
  // exists.
  __ bind(&probe_dictionary);
  // r1: receiver
  // r0: key
  // r3: elements
  __ ldr(r2, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ ldrb(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
  GenerateGlobalInstanceTypeCheck(masm, r2, &slow);
  // Load the property to r0.
  GenerateDictionaryLoad(masm, &slow, r3, r0, r0, r2, r4);
  __ IncrementCounter(&Counters::keyed_load_generic_symbol, 1, r2, r3);
  __ Ret();

  __ bind(&index_string);
  __ IndexFromHash(r3, key);
  // Now jump to the place where smi keys are handled.
  __ jmp(&index_smi);
}


void KeyedLoadIC::GenerateString(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key (index)
  //  -- r1     : receiver
  // -----------------------------------
  Label miss;

  Register receiver = r1;
  Register index = r0;
  Register scratch1 = r2;
  Register scratch2 = r3;
  Register result = r0;

  StringCharAtGenerator char_at_generator(receiver,
                                          index,
                                          scratch1,
                                          scratch2,
                                          result,
                                          &miss,  // When not a string.
                                          &miss,  // When not a number.
                                          &miss,  // When index out of range.
                                          STRING_INDEX_IS_ARRAY_INDEX);
  char_at_generator.GenerateFast(masm);
  __ Ret();

  StubRuntimeCallHelper call_helper;
  char_at_generator.GenerateSlow(masm, call_helper);

  __ bind(&miss);
  GenerateMiss(masm);
}


void KeyedLoadIC::GenerateIndexedInterceptor(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------
  Label slow;

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(r1, &slow);

  // Check that the key is an array index, that is Uint32.
  __ tst(r0, Operand(kSmiTagMask | kSmiSignMask));
  __ b(ne, &slow);

  // Get the map of the receiver.
  __ ldr(r2, FieldMemOperand(r1, HeapObject::kMapOffset));

  // Check that it has indexed interceptor and access checks
  // are not enabled for this object.
  __ ldrb(r3, FieldMemOperand(r2, Map::kBitFieldOffset));
  __ and_(r3, r3, Operand(kSlowCaseBitFieldMask));
  __ cmp(r3, Operand(1 << Map::kHasIndexedInterceptor));
  __ b(ne, &slow);

  // Everything is fine, call runtime.
  __ Push(r1, r0);  // Receiver, key.

  // Perform tail call to the entry.
  __ TailCallExternalReference(ExternalReference(
        IC_Utility(kKeyedLoadPropertyWithInterceptor)), 2, 1);

  __ bind(&slow);
  GenerateMiss(masm);
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- r1     : key
  //  -- r2     : receiver
  //  -- lr     : return address
  // -----------------------------------

  // Push receiver, key and value for runtime call.
  __ Push(r2, r1, r0);

  ExternalReference ref = ExternalReference(IC_Utility(kKeyedStoreIC_Miss));
  __ TailCallExternalReference(ref, 3, 1);
}


void KeyedStoreIC::GenerateRuntimeSetProperty(MacroAssembler* masm,
                                              StrictModeFlag strict_mode) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- r1     : key
  //  -- r2     : receiver
  //  -- lr     : return address
  // -----------------------------------

  // Push receiver, key and value for runtime call.
  __ Push(r2, r1, r0);

  __ mov(r1, Operand(Smi::FromInt(NONE)));          // PropertyAttributes
  __ mov(r0, Operand(Smi::FromInt(strict_mode)));   // Strict mode.
  __ Push(r1, r0);

  __ TailCallRuntime(Runtime::kSetProperty, 5, 1);
}


void KeyedStoreIC::GenerateGeneric(MacroAssembler* masm,
                                   StrictModeFlag strict_mode) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- r1     : key
  //  -- r2     : receiver
  //  -- lr     : return address
  // -----------------------------------
  Label slow, fast, array, extra, check_pixel_array;

  // Register usage.
  Register value = r0;
  Register key = r1;
  Register receiver = r2;
  Register elements = r3;  // Elements array of the receiver.
  // r4 and r5 are used as general scratch registers.

  // Check that the key is a smi.
  __ tst(key, Operand(kSmiTagMask));
  __ b(ne, &slow);
  // Check that the object isn't a smi.
  __ tst(receiver, Operand(kSmiTagMask));
  __ b(eq, &slow);
  // Get the map of the object.
  __ ldr(r4, FieldMemOperand(receiver, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.  We need
  // to do this because this generic stub does not perform map checks.
  __ ldrb(ip, FieldMemOperand(r4, Map::kBitFieldOffset));
  __ tst(ip, Operand(1 << Map::kIsAccessCheckNeeded));
  __ b(ne, &slow);
  // Check if the object is a JS array or not.
  __ ldrb(r4, FieldMemOperand(r4, Map::kInstanceTypeOffset));
  __ cmp(r4, Operand(JS_ARRAY_TYPE));
  __ b(eq, &array);
  // Check that the object is some kind of JS object.
  __ cmp(r4, Operand(FIRST_JS_OBJECT_TYPE));
  __ b(lt, &slow);

  // Object case: Check key against length in the elements array.
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  // Check that the object is in fast mode and writable.
  __ ldr(r4, FieldMemOperand(elements, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kFixedArrayMapRootIndex);
  __ cmp(r4, ip);
  __ b(ne, &check_pixel_array);
  // Check array bounds. Both the key and the length of FixedArray are smis.
  __ ldr(ip, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ cmp(key, Operand(ip));
  __ b(lo, &fast);

  // Slow case, handle jump to runtime.
  __ bind(&slow);
  // Entry registers are intact.
  // r0: value.
  // r1: key.
  // r2: receiver.
  GenerateRuntimeSetProperty(masm, strict_mode);

  // Check whether the elements is a pixel array.
  // r4: elements map.
  __ bind(&check_pixel_array);
  GenerateFastPixelArrayStore(masm,
                              r2,
                              r1,
                              r0,
                              elements,
                              r4,
                              r5,
                              r6,
                              false,
                              false,
                              NULL,
                              &slow,
                              &slow,
                              &slow);

  // Extra capacity case: Check if there is extra capacity to
  // perform the store and update the length. Used for adding one
  // element to the array by writing to array[array.length].
  __ bind(&extra);
  // Condition code from comparing key and array length is still available.
  __ b(ne, &slow);  // Only support writing to writing to array[array.length].
  // Check for room in the elements backing store.
  // Both the key and the length of FixedArray are smis.
  __ ldr(ip, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ cmp(key, Operand(ip));
  __ b(hs, &slow);
  // Calculate key + 1 as smi.
  ASSERT_EQ(0, kSmiTag);
  __ add(r4, key, Operand(Smi::FromInt(1)));
  __ str(r4, FieldMemOperand(receiver, JSArray::kLengthOffset));
  __ b(&fast);

  // Array case: Get the length and the elements array from the JS
  // array. Check that the array is in fast mode (and writable); if it
  // is the length is always a smi.
  __ bind(&array);
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ ldr(r4, FieldMemOperand(elements, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kFixedArrayMapRootIndex);
  __ cmp(r4, ip);
  __ b(ne, &slow);

  // Check the key against the length in the array.
  __ ldr(ip, FieldMemOperand(receiver, JSArray::kLengthOffset));
  __ cmp(key, Operand(ip));
  __ b(hs, &extra);
  // Fall through to fast case.

  __ bind(&fast);
  // Fast case, store the value to the elements backing store.
  __ add(r5, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ add(r5, r5, Operand(key, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ str(value, MemOperand(r5));
  // Skip write barrier if the written value is a smi.
  __ tst(value, Operand(kSmiTagMask));
  __ Ret(eq);
  // Update write barrier for the elements array address.
  __ sub(r4, r5, Operand(elements));
  __ RecordWrite(elements, Operand(r4), r5, r6);

  __ Ret();
}


void StoreIC::GenerateMegamorphic(MacroAssembler* masm,
                                  StrictModeFlag strict_mode) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  // Get the receiver from the stack and probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(Code::STORE_IC,
                                         NOT_IN_LOOP,
                                         MONOMORPHIC,
                                         strict_mode);
  StubCache::GenerateProbe(masm, flags, r1, r2, r3, r4, r5);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
}


void StoreIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  __ Push(r1, r2, r0);

  // Perform tail call to the entry.
  ExternalReference ref = ExternalReference(IC_Utility(kStoreIC_Miss));
  __ TailCallExternalReference(ref, 3, 1);
}


void StoreIC::GenerateArrayLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  //
  // This accepts as a receiver anything JSObject::SetElementsLength accepts
  // (currently anything except for external and pixel arrays which means
  // anything with elements of FixedArray type.), but currently is restricted
  // to JSArray.
  // Value must be a number, but only smis are accepted as the most common case.

  Label miss;

  Register receiver = r1;
  Register value = r0;
  Register scratch = r3;

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &miss);

  // Check that the object is a JS array.
  __ CompareObjectType(receiver, scratch, scratch, JS_ARRAY_TYPE);
  __ b(ne, &miss);

  // Check that elements are FixedArray.
  // We rely on StoreIC_ArrayLength below to deal with all types of
  // fast elements (including COW).
  __ ldr(scratch, FieldMemOperand(receiver, JSArray::kElementsOffset));
  __ CompareObjectType(scratch, scratch, scratch, FIXED_ARRAY_TYPE);
  __ b(ne, &miss);

  // Check that value is a smi.
  __ JumpIfNotSmi(value, &miss);

  // Prepare tail call to StoreIC_ArrayLength.
  __ Push(receiver, value);

  ExternalReference ref = ExternalReference(IC_Utility(kStoreIC_ArrayLength));
  __ TailCallExternalReference(ref, 2, 1);

  __ bind(&miss);

  GenerateMiss(masm);
}


void StoreIC::GenerateNormal(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label miss;

  GenerateStringDictionaryReceiverCheck(masm, r1, r3, r4, r5, &miss);

  GenerateDictionaryStore(masm, &miss, r3, r2, r0, r4, r5);
  __ IncrementCounter(&Counters::store_normal_hit, 1, r4, r5);
  __ Ret();

  __ bind(&miss);
  __ IncrementCounter(&Counters::store_normal_miss, 1, r4, r5);
  GenerateMiss(masm);
}


void StoreIC::GenerateGlobalProxy(MacroAssembler* masm,
                                  StrictModeFlag strict_mode) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : receiver
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  __ Push(r1, r2, r0);

  __ mov(r1, Operand(Smi::FromInt(NONE)));  // PropertyAttributes
  __ mov(r0, Operand(Smi::FromInt(strict_mode)));
  __ Push(r1, r0);

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty, 5, 1);
}


#undef __


Condition CompareIC::ComputeCondition(Token::Value op) {
  switch (op) {
    case Token::EQ_STRICT:
    case Token::EQ:
      return eq;
    case Token::LT:
      return lt;
    case Token::GT:
      // Reverse left and right operands to obtain ECMA-262 conversion order.
      return lt;
    case Token::LTE:
      // Reverse left and right operands to obtain ECMA-262 conversion order.
      return ge;
    case Token::GTE:
      return ge;
    default:
      UNREACHABLE();
      return kNoCondition;
  }
}


void CompareIC::UpdateCaches(Handle<Object> x, Handle<Object> y) {
  HandleScope scope;
  Handle<Code> rewritten;
  State previous_state = GetState();
  State state = TargetState(previous_state, false, x, y);
  if (state == GENERIC) {
    CompareStub stub(GetCondition(), strict(), NO_COMPARE_FLAGS, r1, r0);
    rewritten = stub.GetCode();
  } else {
    ICCompareStub stub(op_, state);
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
  Address cmp_instruction_address =
      address + Assembler::kCallTargetAddressOffset;

  // If the instruction following the call is not a cmp rx, #yyy, nothing
  // was inlined.
  Instr instr = Assembler::instr_at(cmp_instruction_address);
  if (!Assembler::IsCmpImmediate(instr)) {
    return;
  }

  // The delta to the start of the map check instruction and the
  // condition code uses at the patched jump.
  int delta = Assembler::GetCmpImmediateRawImmediate(instr);
  delta +=
      Assembler::GetCmpImmediateRegister(instr).code() * kOff12Mask;
  // If the delta is 0 the instruction is cmp r0, #0 which also signals that
  // nothing was inlined.
  if (delta == 0) {
    return;
  }

#ifdef DEBUG
  if (FLAG_trace_ic) {
    PrintF("[  patching ic at %p, cmp=%p, delta=%d\n",
           address, cmp_instruction_address, delta);
  }
#endif

  Address patch_address =
      cmp_instruction_address - delta * Instruction::kInstrSize;
  Instr instr_at_patch = Assembler::instr_at(patch_address);
  Instr branch_instr =
      Assembler::instr_at(patch_address + Instruction::kInstrSize);
  ASSERT(Assembler::IsCmpRegister(instr_at_patch));
  ASSERT_EQ(Assembler::GetRn(instr_at_patch).code(),
            Assembler::GetRm(instr_at_patch).code());
  ASSERT(Assembler::IsBranch(branch_instr));
  if (Assembler::GetCondition(branch_instr) == eq) {
    // This is patching a "jump if not smi" site to be active.
    // Changing
    //   cmp rx, rx
    //   b eq, <target>
    // to
    //   tst rx, #kSmiTagMask
    //   b ne, <target>
    CodePatcher patcher(patch_address, 2);
    Register reg = Assembler::GetRn(instr_at_patch);
    patcher.masm()->tst(reg, Operand(kSmiTagMask));
    patcher.EmitCondition(ne);
  } else {
    ASSERT(Assembler::GetCondition(branch_instr) == ne);
    // This is patching a "jump if smi" site to be active.
    // Changing
    //   cmp rx, rx
    //   b ne, <target>
    // to
    //   tst rx, #kSmiTagMask
    //   b eq, <target>
    CodePatcher patcher(patch_address, 2);
    Register reg = Assembler::GetRn(instr_at_patch);
    patcher.masm()->tst(reg, Operand(kSmiTagMask));
    patcher.EmitCondition(eq);
  }
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
