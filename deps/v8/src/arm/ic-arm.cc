// Copyright 2011 the V8 project authors. All rights reserved.
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
#include "codegen.h"
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
  __ JumpIfSmi(receiver, miss);

  // Check that the receiver is a valid JS object.
  __ CompareObjectType(receiver, t0, t1, FIRST_SPEC_OBJECT_TYPE);
  __ b(lt, miss);

  // If this assert fails, we have to check upper bound too.
  STATIC_ASSERT(LAST_TYPE == LAST_SPEC_OBJECT_TYPE);

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
  StringDictionaryLookupStub::GeneratePositiveLookup(masm,
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
  __ tst(scratch1, Operand(PropertyDetails::TypeField::kMask << kSmiTagSize));
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
  StringDictionaryLookupStub::GeneratePositiveLookup(masm,
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
  const int kTypeAndReadOnlyMask =
      (PropertyDetails::TypeField::kMask |
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
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize < kPointerSizeLog2);
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
  STATIC_ASSERT(kSymbolTag != 0);
  __ tst(hash, Operand(kIsSymbolMask));
  __ b(eq, not_symbol);
}


// Defined in ic.cc.
Object* CallIC_Miss(Arguments args);

// The generated code does not accept smi keys.
// The generated code falls through if both probes miss.
static void GenerateMonomorphicCacheProbe(MacroAssembler* masm,
                                          int argc,
                                          Code::Kind kind,
                                          Code::ExtraICState extra_ic_state) {
  // ----------- S t a t e -------------
  //  -- r1    : receiver
  //  -- r2    : name
  // -----------------------------------
  Label number, non_number, non_string, boolean, probe, miss;

  // Probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(kind,
                                         MONOMORPHIC,
                                         extra_ic_state,
                                         NORMAL,
                                         argc);
  Isolate::Current()->stub_cache()->GenerateProbe(
      masm, flags, r1, r2, r3, r4, r5);

  // If the stub cache probing failed, the receiver might be a value.
  // For value objects, we use the map of the prototype objects for
  // the corresponding JSValue for the cache and that is what we need
  // to probe.
  //
  // Check for number.
  __ JumpIfSmi(r1, &number);
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
  Isolate::Current()->stub_cache()->GenerateProbe(
      masm, flags, r1, r2, r3, r4, r5);

  __ bind(&miss);
}


static void GenerateFunctionTailCall(MacroAssembler* masm,
                                     int argc,
                                     Label* miss,
                                     Register scratch) {
  // r1: function

  // Check that the value isn't a smi.
  __ JumpIfSmi(r1, miss);

  // Check that the value is a JSFunction.
  __ CompareObjectType(r1, scratch, scratch, JS_FUNCTION_TYPE);
  __ b(ne, miss);

  // Invoke the function.
  ParameterCount actual(argc);
  __ InvokeFunction(r1, actual, JUMP_FUNCTION,
                    NullCallWrapper(), CALL_AS_METHOD);
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


static void GenerateCallMiss(MacroAssembler* masm,
                             int argc,
                             IC::UtilityId id,
                             Code::ExtraICState extra_ic_state) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Isolate* isolate = masm->isolate();

  if (id == IC::kCallIC_Miss) {
    __ IncrementCounter(isolate->counters()->call_miss(), 1, r3, r4);
  } else {
    __ IncrementCounter(isolate->counters()->keyed_call_miss(), 1, r3, r4);
  }

  // Get the receiver of the function from the stack.
  __ ldr(r3, MemOperand(sp, argc * kPointerSize));

  __ EnterInternalFrame();

  // Push the receiver and the name of the function.
  __ Push(r3, r2);

  // Call the entry.
  __ mov(r0, Operand(2));
  __ mov(r1, Operand(ExternalReference(IC_Utility(id), isolate)));

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
    __ JumpIfSmi(r2, &invoke);
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
  CallKind call_kind = CallICBase::Contextual::decode(extra_ic_state)
      ? CALL_AS_FUNCTION
      : CALL_AS_METHOD;
  ParameterCount actual(argc);
  __ InvokeFunction(r1,
                    actual,
                    JUMP_FUNCTION,
                    NullCallWrapper(),
                    call_kind);
}


void CallIC::GenerateMiss(MacroAssembler* masm,
                          int argc,
                          Code::ExtraICState extra_ic_state) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  GenerateCallMiss(masm, argc, IC::kCallIC_Miss, extra_ic_state);
}


void CallIC::GenerateMegamorphic(MacroAssembler* masm,
                                 int argc,
                                 Code::ExtraICState extra_ic_state) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  // Get the receiver of the function from the stack into r1.
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));
  GenerateMonomorphicCacheProbe(masm, argc, Code::CALL_IC, extra_ic_state);
  GenerateMiss(masm, argc, extra_ic_state);
}


void CallIC::GenerateNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  GenerateCallNormal(masm, argc);
  GenerateMiss(masm, argc, Code::kNoExtraICState);
}


void KeyedCallIC::GenerateMiss(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------

  GenerateCallMiss(masm, argc, IC::kKeyedCallIC_Miss, Code::kNoExtraICState);
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
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->keyed_call_generic_smi_fast(), 1, r0, r3);

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
  __ LoadFromNumberDictionary(&slow_load, r4, r2, r1, r0, r3, r5);
  __ IncrementCounter(counters->keyed_call_generic_smi_dict(), 1, r0, r3);
  __ jmp(&do_call);

  __ bind(&slow_load);
  // This branch is taken when calling KeyedCallIC_Miss is neither required
  // nor beneficial.
  __ IncrementCounter(counters->keyed_call_generic_slow_load(), 1, r0, r3);
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
  __ IncrementCounter(counters->keyed_call_generic_lookup_dict(), 1, r0, r3);
  __ jmp(&do_call);

  __ bind(&lookup_monomorphic_cache);
  __ IncrementCounter(counters->keyed_call_generic_lookup_cache(), 1, r0, r3);
  GenerateMonomorphicCacheProbe(masm,
                                argc,
                                Code::KEYED_CALL_IC,
                                Code::kNoExtraICState);
  // Fall through on miss.

  __ bind(&slow_call);
  // This branch is taken if:
  // - the receiver requires boxing or access check,
  // - the key is neither smi nor symbol,
  // - the value loaded is not a function,
  // - there is hope that the runtime will create a monomorphic call stub
  //   that will get fetched next time.
  __ IncrementCounter(counters->keyed_call_generic_slow(), 1, r0, r3);
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
  __ JumpIfSmi(r2, &miss);
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
  Code::Flags flags =
      Code::ComputeFlags(Code::LOAD_IC, MONOMORPHIC);
  Isolate::Current()->stub_cache()->GenerateProbe(
      masm, flags, r0, r2, r3, r4, r5);

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
  Isolate* isolate = masm->isolate();

  __ IncrementCounter(isolate->counters()->load_miss(), 1, r3, r4);

  __ mov(r3, r0);
  __ Push(r3, r2);

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kLoadIC_Miss), isolate);
  __ TailCallExternalReference(ref, 2, 1);
}


static MemOperand GenerateMappedArgumentsLookup(MacroAssembler* masm,
                                                Register object,
                                                Register key,
                                                Register scratch1,
                                                Register scratch2,
                                                Register scratch3,
                                                Label* unmapped_case,
                                                Label* slow_case) {
  Heap* heap = masm->isolate()->heap();

  // Check that the receiver is a JSObject. Because of the map check
  // later, we do not need to check for interceptors or whether it
  // requires access checks.
  __ JumpIfSmi(object, slow_case);
  // Check that the object is some kind of JSObject.
  __ CompareObjectType(object, scratch1, scratch2, FIRST_JS_RECEIVER_TYPE);
  __ b(lt, slow_case);

  // Check that the key is a positive smi.
  __ tst(key, Operand(0x8000001));
  __ b(ne, slow_case);

  // Load the elements into scratch1 and check its map.
  Handle<Map> arguments_map(heap->non_strict_arguments_elements_map());
  __ ldr(scratch1, FieldMemOperand(object, JSObject::kElementsOffset));
  __ CheckMap(scratch1, scratch2, arguments_map, slow_case, DONT_DO_SMI_CHECK);

  // Check if element is in the range of mapped arguments. If not, jump
  // to the unmapped lookup with the parameter map in scratch1.
  __ ldr(scratch2, FieldMemOperand(scratch1, FixedArray::kLengthOffset));
  __ sub(scratch2, scratch2, Operand(Smi::FromInt(2)));
  __ cmp(key, Operand(scratch2));
  __ b(cs, unmapped_case);

  // Load element index and check whether it is the hole.
  const int kOffset =
      FixedArray::kHeaderSize + 2 * kPointerSize - kHeapObjectTag;

  __ mov(scratch3, Operand(kPointerSize >> 1));
  __ mul(scratch3, key, scratch3);
  __ add(scratch3, scratch3, Operand(kOffset));

  __ ldr(scratch2, MemOperand(scratch1, scratch3));
  __ LoadRoot(scratch3, Heap::kTheHoleValueRootIndex);
  __ cmp(scratch2, scratch3);
  __ b(eq, unmapped_case);

  // Load value from context and return it. We can reuse scratch1 because
  // we do not jump to the unmapped lookup (which requires the parameter
  // map in scratch1).
  __ ldr(scratch1, FieldMemOperand(scratch1, FixedArray::kHeaderSize));
  __ mov(scratch3, Operand(kPointerSize >> 1));
  __ mul(scratch3, scratch2, scratch3);
  __ add(scratch3, scratch3, Operand(Context::kHeaderSize - kHeapObjectTag));
  return MemOperand(scratch1, scratch3);
}


static MemOperand GenerateUnmappedArgumentsLookup(MacroAssembler* masm,
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
  __ ldr(backing_store, FieldMemOperand(parameter_map, kBackingStoreOffset));
  Handle<Map> fixed_array_map(masm->isolate()->heap()->fixed_array_map());
  __ CheckMap(backing_store, scratch, fixed_array_map, slow_case,
              DONT_DO_SMI_CHECK);
  __ ldr(scratch, FieldMemOperand(backing_store, FixedArray::kLengthOffset));
  __ cmp(key, Operand(scratch));
  __ b(cs, slow_case);
  __ mov(scratch, Operand(kPointerSize >> 1));
  __ mul(scratch, key, scratch);
  __ add(scratch,
         scratch,
         Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  return MemOperand(backing_store, scratch);
}


void KeyedLoadIC::GenerateNonStrictArguments(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------
  Label slow, notin;
  MemOperand mapped_location =
      GenerateMappedArgumentsLookup(masm, r1, r0, r2, r3, r4, &notin, &slow);
  __ ldr(r0, mapped_location);
  __ Ret();
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in r2.
  MemOperand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, r0, r2, r3, &slow);
  __ ldr(r2, unmapped_location);
  __ LoadRoot(r3, Heap::kTheHoleValueRootIndex);
  __ cmp(r2, r3);
  __ b(eq, &slow);
  __ mov(r0, r2);
  __ Ret();
  __ bind(&slow);
  GenerateMiss(masm, false);
}


void KeyedStoreIC::GenerateNonStrictArguments(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- r1     : key
  //  -- r2     : receiver
  //  -- lr     : return address
  // -----------------------------------
  Label slow, notin;
  MemOperand mapped_location =
      GenerateMappedArgumentsLookup(masm, r2, r1, r3, r4, r5, &notin, &slow);
  __ str(r0, mapped_location);
  __ add(r6, r3, r5);
  __ RecordWrite(r3, r6, r9);
  __ Ret();
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in r3.
  MemOperand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, r1, r3, r4, &slow);
  __ str(r0, unmapped_location);
  __ add(r6, r3, r4);
  __ RecordWrite(r3, r6, r9);
  __ Ret();
  __ bind(&slow);
  GenerateMiss(masm, false);
}


void KeyedCallIC::GenerateNonStrictArguments(MacroAssembler* masm,
                                             int argc) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  // -----------------------------------
  Label slow, notin;
  // Load receiver.
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));
  MemOperand mapped_location =
      GenerateMappedArgumentsLookup(masm, r1, r2, r3, r4, r5, &notin, &slow);
  __ ldr(r1, mapped_location);
  GenerateFunctionTailCall(masm, argc, &slow, r3);
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in r3.
  MemOperand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, r2, r3, r4, &slow);
  __ ldr(r1, unmapped_location);
  __ LoadRoot(r3, Heap::kTheHoleValueRootIndex);
  __ cmp(r1, r3);
  __ b(eq, &slow);
  GenerateFunctionTailCall(masm, argc, &slow, r3);
  __ bind(&slow);
  GenerateMiss(masm, argc);
}


Object* KeyedLoadIC_Miss(Arguments args);


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm, bool force_generic) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- r0     : key
  //  -- r1     : receiver
  // -----------------------------------
  Isolate* isolate = masm->isolate();

  __ IncrementCounter(isolate->counters()->keyed_load_miss(), 1, r3, r4);

  __ Push(r1, r0);

  // Perform tail call to the entry.
  ExternalReference ref = force_generic
      ? ExternalReference(IC_Utility(kKeyedLoadIC_MissForceGeneric), isolate)
      : ExternalReference(IC_Utility(kKeyedLoadIC_Miss), isolate);

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
  Label probe_dictionary, check_number_dictionary;

  Register key = r0;
  Register receiver = r1;

  Isolate* isolate = masm->isolate();

  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &check_string);
  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from below
  // where a numeric string is converted to a smi.

  GenerateKeyedLoadReceiverCheck(
      masm, receiver, r2, r3, Map::kHasIndexedInterceptor, &slow);

  // Check the receiver's map to see if it has fast elements.
  __ CheckFastElements(r2, r3, &check_number_dictionary);

  GenerateFastArrayLoad(
      masm, receiver, key, r4, r3, r2, r0, NULL, &slow);
  __ IncrementCounter(isolate->counters()->keyed_load_generic_smi(), 1, r2, r3);
  __ Ret();

  __ bind(&check_number_dictionary);
  __ ldr(r4, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ ldr(r3, FieldMemOperand(r4, JSObject::kMapOffset));

  // Check whether the elements is a number dictionary.
  // r0: key
  // r3: elements map
  // r4: elements
  __ LoadRoot(ip, Heap::kHashTableMapRootIndex);
  __ cmp(r3, ip);
  __ b(ne, &slow);
  __ mov(r2, Operand(r0, ASR, kSmiTagSize));
  __ LoadFromNumberDictionary(&slow, r4, r0, r0, r2, r3, r5);
  __ Ret();

  // Slow case, key and receiver still in r0 and r1.
  __ bind(&slow);
  __ IncrementCounter(isolate->counters()->keyed_load_generic_slow(),
                      1, r2, r3);
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
  ExternalReference cache_keys =
      ExternalReference::keyed_lookup_cache_keys(isolate);
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
  ExternalReference cache_field_offsets =
      ExternalReference::keyed_lookup_cache_field_offsets(isolate);
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
  __ IncrementCounter(isolate->counters()->keyed_load_generic_lookup_cache(),
                      1, r2, r3);
  __ Ret();

  // Load property array property.
  __ bind(&property_array_property);
  __ ldr(r1, FieldMemOperand(r1, JSObject::kPropertiesOffset));
  __ add(r1, r1, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ ldr(r0, MemOperand(r1, r5, LSL, kPointerSizeLog2));
  __ IncrementCounter(isolate->counters()->keyed_load_generic_lookup_cache(),
                      1, r2, r3);
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
  __ IncrementCounter(isolate->counters()->keyed_load_generic_symbol(),
                      1, r2, r3);
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
  GenerateMiss(masm, false);
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
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(kKeyedLoadPropertyWithInterceptor),
                        masm->isolate()),
      2,
      1);

  __ bind(&slow);
  GenerateMiss(masm, false);
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm, bool force_generic) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- r1     : key
  //  -- r2     : receiver
  //  -- lr     : return address
  // -----------------------------------

  // Push receiver, key and value for runtime call.
  __ Push(r2, r1, r0);

  ExternalReference ref = force_generic
      ? ExternalReference(IC_Utility(kKeyedStoreIC_MissForceGeneric),
                          masm->isolate())
      : ExternalReference(IC_Utility(kKeyedStoreIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void KeyedStoreIC::GenerateSlow(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- r1     : key
  //  -- r2     : receiver
  //  -- lr     : return address
  // -----------------------------------

  // Push receiver, key and value for runtime call.
  __ Push(r2, r1, r0);

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  ExternalReference ref =
      ExternalReference(IC_Utility(kKeyedStoreIC_Slow), masm->isolate());
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
  Label slow, fast, array, extra;

  // Register usage.
  Register value = r0;
  Register key = r1;
  Register receiver = r2;
  Register elements = r3;  // Elements array of the receiver.
  // r4 and r5 are used as general scratch registers.

  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &slow);
  // Check that the object isn't a smi.
  __ JumpIfSmi(receiver, &slow);
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
  // Check that the object is some kind of JSObject.
  __ cmp(r4, Operand(FIRST_JS_RECEIVER_TYPE));
  __ b(lt, &slow);
  __ cmp(r4, Operand(JS_PROXY_TYPE));
  __ b(eq, &slow);
  __ cmp(r4, Operand(JS_FUNCTION_PROXY_TYPE));
  __ b(eq, &slow);

  // Object case: Check key against length in the elements array.
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  // Check that the object is in fast mode and writable.
  __ ldr(r4, FieldMemOperand(elements, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kFixedArrayMapRootIndex);
  __ cmp(r4, ip);
  __ b(ne, &slow);
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
  STATIC_ASSERT(kSmiTag == 0);
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
  Code::Flags flags =
      Code::ComputeFlags(Code::STORE_IC, MONOMORPHIC, strict_mode);

  Isolate::Current()->stub_cache()->GenerateProbe(
      masm, flags, r1, r2, r3, r4, r5);

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
  ExternalReference ref =
      ExternalReference(IC_Utility(kStoreIC_Miss), masm->isolate());
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

  ExternalReference ref =
      ExternalReference(IC_Utility(kStoreIC_ArrayLength), masm->isolate());
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
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->store_normal_hit(),
                      1, r4, r5);
  __ Ret();

  __ bind(&miss);
  __ IncrementCounter(counters->store_normal_miss(), 1, r4, r5);
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
