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

namespace v8 {
namespace internal {


// ----------------------------------------------------------------------------
// Static IC stub generators.
//

#define __ ACCESS_MASM(masm)


// Helper function used from LoadIC/CallIC GenerateNormal.
static void GenerateDictionaryLoad(MacroAssembler* masm,
                                   Label* miss,
                                   Register t0,
                                   Register t1) {
  // Register use:
  //
  // t0 - used to hold the property dictionary.
  //
  // t1 - initially the receiver
  //    - used for the index into the property dictionary
  //    - holds the result on exit.
  //
  // r3 - used as temporary and to hold the capacity of the property
  //      dictionary.
  //
  // r2 - holds the name of the property and is unchanges.

  Label done;

  // Check for the absence of an interceptor.
  // Load the map into t0.
  __ ldr(t0, FieldMemOperand(t1, JSObject::kMapOffset));
  // Test the has_named_interceptor bit in the map.
  __ ldr(t0, FieldMemOperand(t1, Map::kInstanceAttributesOffset));
  __ tst(t0, Operand(1 << (Map::kHasNamedInterceptor + (3 * 8))));
  // Jump to miss if the interceptor bit is set.
  __ b(ne, miss);


  // Check that the properties array is a dictionary.
  __ ldr(t0, FieldMemOperand(t1, JSObject::kPropertiesOffset));
  __ ldr(r3, FieldMemOperand(t0, HeapObject::kMapOffset));
  __ cmp(r3, Operand(Factory::hash_table_map()));
  __ b(ne, miss);

  // Compute the capacity mask.
  const int kCapacityOffset =
      Array::kHeaderSize + Dictionary::kCapacityIndex * kPointerSize;
  __ ldr(r3, FieldMemOperand(t0, kCapacityOffset));
  __ mov(r3, Operand(r3, ASR, kSmiTagSize));  // convert smi to int
  __ sub(r3, r3, Operand(1));

  const int kElementsStartOffset =
      Array::kHeaderSize + Dictionary::kElementsStartIndex * kPointerSize;

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  static const int kProbes = 4;
  for (int i = 0; i < kProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ ldr(t1, FieldMemOperand(r2, String::kLengthOffset));
    __ mov(t1, Operand(t1, LSR, String::kHashShift));
    if (i > 0) {
      __ add(t1, t1, Operand(Dictionary::GetProbeOffset(i)));
    }
    __ and_(t1, t1, Operand(r3));

    // Scale the index by multiplying by the element size.
    ASSERT(Dictionary::kElementSize == 3);
    __ add(t1, t1, Operand(t1, LSL, 1));  // t1 = t1 * 3

    // Check if the key is identical to the name.
    __ add(t1, t0, Operand(t1, LSL, 2));
    __ ldr(ip, FieldMemOperand(t1, kElementsStartOffset));
    __ cmp(r2, Operand(ip));
    if (i != kProbes - 1) {
      __ b(eq, &done);
    } else {
      __ b(ne, miss);
    }
  }

  // Check that the value is a normal property.
  __ bind(&done);  // t1 == t0 + 4*index
  __ ldr(r3, FieldMemOperand(t1, kElementsStartOffset + 2 * kPointerSize));
  __ tst(r3, Operand(PropertyDetails::TypeField::mask() << kSmiTagSize));
  __ b(ne, miss);

  // Get the value at the masked, scaled index and return.
  __ ldr(t1, FieldMemOperand(t1, kElementsStartOffset + 1 * kPointerSize));
}


// Helper function used to check that a value is either not an object
// or is loaded if it is an object.
static void GenerateCheckNonObjectOrLoaded(MacroAssembler* masm,
                                           Label* miss,
                                           Register value,
                                           Register scratch) {
  Label done;
  // Check if the value is a Smi.
  __ tst(value, Operand(kSmiTagMask));
  __ b(eq, &done);
  // Check if the object has been loaded.
  __ ldr(scratch, FieldMemOperand(value, JSObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(scratch, Map::kBitField2Offset));
  __ tst(scratch, Operand(1 << Map::kNeedsLoading));
  __ b(ne, miss);
  __ bind(&done);
}


void LoadIC::GenerateArrayLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------

  Label miss;

  __ ldr(r0, MemOperand(sp, 0));

  StubCompiler::GenerateLoadArrayLength(masm, r0, r3, &miss);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateStringLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------
  Label miss;

  __ ldr(r0, MemOperand(sp, 0));

  StubCompiler::GenerateLoadStringLength2(masm, r0, r1, r3, &miss);
  // Cache miss: Jump to runtime.
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateFunctionPrototype(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------

  // NOTE: Right now, this code always misses on ARM which is
  // sub-optimal. We should port the fast case code from IA-32.

  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Miss));
  __ Jump(ic, RelocInfo::CODE_TARGET);
}


// Defined in ic.cc.
Object* CallIC_Miss(Arguments args);

void CallIC::GenerateMegamorphic(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- lr: return address
  // -----------------------------------
  Label number, non_number, non_string, boolean, probe, miss;

  // Get the receiver of the function from the stack into r1.
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));
  // Get the name of the function from the stack; 1 ~ receiver.
  __ ldr(r2, MemOperand(sp, (argc + 1) * kPointerSize));

  // Probe the stub cache.
  Code::Flags flags =
      Code::ComputeFlags(Code::CALL_IC, NOT_IN_LOOP, MONOMORPHIC, NORMAL, argc);
  StubCache::GenerateProbe(masm, flags, r1, r2, r3);

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
  __ cmp(r1, Operand(Factory::true_value()));
  __ b(eq, &boolean);
  __ cmp(r1, Operand(Factory::false_value()));
  __ b(ne, &miss);
  __ bind(&boolean);
  StubCompiler::GenerateLoadGlobalFunctionPrototype(
      masm, Context::BOOLEAN_FUNCTION_INDEX, r1);

  // Probe the stub cache for the value object.
  __ bind(&probe);
  StubCache::GenerateProbe(masm, flags, r1, r2, r3);

  // Cache miss: Jump to runtime.
  __ bind(&miss);
  Generate(masm, argc, ExternalReference(IC_Utility(kCallIC_Miss)));
}


static void GenerateNormalHelper(MacroAssembler* masm,
                                 int argc,
                                 bool is_global_object,
                                 Label* miss) {
  // Search dictionary - put result in register r1.
  GenerateDictionaryLoad(masm, miss, r0, r1);

  // Check that the value isn't a smi.
  __ tst(r1, Operand(kSmiTagMask));
  __ b(eq, miss);

  // Check that the value is a JSFunction.
  __ CompareObjectType(r1, r0, r0, JS_FUNCTION_TYPE);
  __ b(ne, miss);

  // Check that the function has been loaded.
  __ ldr(r0, FieldMemOperand(r1, JSObject::kMapOffset));
  __ ldrb(r0, FieldMemOperand(r0, Map::kBitField2Offset));
  __ tst(r0, Operand(1 << Map::kNeedsLoading));
  __ b(ne, miss);

  // Patch the receiver with the global proxy if necessary.
  if (is_global_object) {
    __ ldr(r2, MemOperand(sp, argc * kPointerSize));
    __ ldr(r2, FieldMemOperand(r2, GlobalObject::kGlobalReceiverOffset));
    __ str(r2, MemOperand(sp, argc * kPointerSize));
  }

  // Invoke the function.
  ParameterCount actual(argc);
  __ InvokeFunction(r1, actual, JUMP_FUNCTION);
}


void CallIC::GenerateNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  //  -- lr: return address
  // -----------------------------------

  Label miss, global_object, non_global_object;

  // Get the receiver of the function from the stack into r1.
  __ ldr(r1, MemOperand(sp, argc * kPointerSize));
  // Get the name of the function from the stack; 1 ~ receiver.
  __ ldr(r2, MemOperand(sp, (argc + 1) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ tst(r1, Operand(kSmiTagMask));
  __ b(eq, &miss);

  // Check that the receiver is a valid JS object.  Put the map in r3.
  __ CompareObjectType(r1, r3, r0, FIRST_JS_OBJECT_TYPE);
  __ b(lt, &miss);

  // If this assert fails, we have to check upper bound too.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);

  // Check for access to global object.
  __ cmp(r0, Operand(JS_GLOBAL_OBJECT_TYPE));
  __ b(eq, &global_object);
  __ cmp(r0, Operand(JS_BUILTINS_OBJECT_TYPE));
  __ b(ne, &non_global_object);

  // Accessing global object: Load and invoke.
  __ bind(&global_object);
  // Check that the global object does not require access checks.
  __ ldrb(r3, FieldMemOperand(r3, Map::kBitFieldOffset));
  __ tst(r3, Operand(1 << Map::kIsAccessCheckNeeded));
  __ b(ne, &miss);
  GenerateNormalHelper(masm, argc, true, &miss);

  // Accessing non-global object: Check for access to global proxy.
  Label global_proxy, invoke;
  __ bind(&non_global_object);
  __ cmp(r0, Operand(JS_GLOBAL_PROXY_TYPE));
  __ b(eq, &global_proxy);
  // Check that the non-global, non-global-proxy object does not
  // require access checks.
  __ ldrb(r3, FieldMemOperand(r3, Map::kBitFieldOffset));
  __ tst(r3, Operand(1 << Map::kIsAccessCheckNeeded));
  __ b(ne, &miss);
  __ bind(&invoke);
  GenerateNormalHelper(masm, argc, false, &miss);

  // Global object access: Check access rights.
  __ bind(&global_proxy);
  __ CheckAccessGlobalProxy(r1, r0, &miss);
  __ b(&invoke);

  // Cache miss: Jump to runtime.
  __ bind(&miss);
  Generate(masm, argc, ExternalReference(IC_Utility(kCallIC_Miss)));
}


void CallIC::Generate(MacroAssembler* masm,
                      int argc,
                      const ExternalReference& f) {
  // ----------- S t a t e -------------
  //  -- lr: return address
  // -----------------------------------

  // Get the receiver of the function from the stack.
  __ ldr(r2, MemOperand(sp, argc * kPointerSize));
  // Get the name of the function to call from the stack.
  __ ldr(r1, MemOperand(sp, (argc + 1) * kPointerSize));

  __ EnterInternalFrame();

  // Push the receiver and the name of the function.
  __ stm(db_w, sp, r1.bit() | r2.bit());

  // Call the entry.
  __ mov(r0, Operand(2));
  __ mov(r1, Operand(f));

  CEntryStub stub;
  __ CallStub(&stub);

  // Move result to r1 and leave the internal frame.
  __ mov(r1, Operand(r0));
  __ LeaveInternalFrame();

  // Check if the receiver is a global object of some sort.
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

  // Invoke the function.
  ParameterCount actual(argc);
  __ bind(&invoke);
  __ InvokeFunction(r1, actual, JUMP_FUNCTION);
}


// Defined in ic.cc.
Object* LoadIC_Miss(Arguments args);

void LoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------

  __ ldr(r0, MemOperand(sp, 0));
  // Probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(Code::LOAD_IC,
                                         NOT_IN_LOOP,
                                         MONOMORPHIC);
  StubCache::GenerateProbe(masm, flags, r0, r2, r3);

  // Cache miss: Jump to runtime.
  Generate(masm, ExternalReference(IC_Utility(kLoadIC_Miss)));
}


void LoadIC::GenerateNormal(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------

  Label miss, probe, global;

  __ ldr(r0, MemOperand(sp, 0));
  // Check that the receiver isn't a smi.
  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &miss);

  // Check that the receiver is a valid JS object.  Put the map in r3.
  __ CompareObjectType(r0, r3, r1, FIRST_JS_OBJECT_TYPE);
  __ b(lt, &miss);
  // If this assert fails, we have to check upper bound too.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);

  // Check for access to global object (unlikely).
  __ cmp(r1, Operand(JS_GLOBAL_PROXY_TYPE));
  __ b(eq, &global);

  // Check for non-global object that requires access check.
  __ ldrb(r3, FieldMemOperand(r3, Map::kBitFieldOffset));
  __ tst(r3, Operand(1 << Map::kIsAccessCheckNeeded));
  __ b(ne, &miss);

  __ bind(&probe);
  GenerateDictionaryLoad(masm, &miss, r1, r0);
  GenerateCheckNonObjectOrLoaded(masm, &miss, r0, r1);
  __ Ret();

  // Global object access: Check access rights.
  __ bind(&global);
  __ CheckAccessGlobalProxy(r0, r1, &miss);
  __ b(&probe);

  // Cache miss: Restore receiver from stack and jump to runtime.
  __ bind(&miss);
  Generate(masm, ExternalReference(IC_Utility(kLoadIC_Miss)));
}


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  Generate(masm, ExternalReference(IC_Utility(kLoadIC_Miss)));
}


void LoadIC::Generate(MacroAssembler* masm, const ExternalReference& f) {
  // ----------- S t a t e -------------
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------

  __ ldr(r3, MemOperand(sp, 0));
  __ stm(db_w, sp, r2.bit() | r3.bit());

  // Perform tail call to the entry.
  __ TailCallRuntime(f, 2);
}


// TODO(181): Implement map patching once loop nesting is tracked on the
// ARM platform so we can generate inlined fast-case code loads in
// loops.
void LoadIC::ClearInlinedVersion(Address address) {}
bool LoadIC::PatchInlinedLoad(Address address, Object* map, int offset) {
  return false;
}

void KeyedLoadIC::ClearInlinedVersion(Address address) {}
bool KeyedLoadIC::PatchInlinedLoad(Address address, Object* map) {
  return false;
}

void KeyedStoreIC::ClearInlinedVersion(Address address) {}
void KeyedStoreIC::RestoreInlinedVersion(Address address) {}
bool KeyedStoreIC::PatchInlinedStore(Address address, Object* map) {
  return false;
}


Object* KeyedLoadIC_Miss(Arguments args);


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  Generate(masm, ExternalReference(IC_Utility(kKeyedLoadIC_Miss)));
}


void KeyedLoadIC::Generate(MacroAssembler* masm, const ExternalReference& f) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- sp[0]  : key
  //  -- sp[4]  : receiver
  __ ldm(ia, sp, r2.bit() | r3.bit());
  __ stm(db_w, sp, r2.bit() | r3.bit());

  __ TailCallRuntime(f, 2);
}


void KeyedLoadIC::GenerateGeneric(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- lr     : return address
  //  -- sp[0]  : key
  //  -- sp[4]  : receiver
  Label slow, fast;

  // Get the key and receiver object from the stack.
  __ ldm(ia, sp, r0.bit() | r1.bit());
  // Check that the key is a smi.
  __ tst(r0, Operand(kSmiTagMask));
  __ b(ne, &slow);
  __ mov(r0, Operand(r0, ASR, kSmiTagSize));
  // Check that the object isn't a smi.
  __ tst(r1, Operand(kSmiTagMask));
  __ b(eq, &slow);

  // Get the map of the receiver.
  __ ldr(r2, FieldMemOperand(r1, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.  We need
  // to check this explicitly since this generic stub does not perform
  // map checks.
  __ ldrb(r3, FieldMemOperand(r2, Map::kBitFieldOffset));
  __ tst(r3, Operand(1 << Map::kIsAccessCheckNeeded));
  __ b(ne, &slow);
  // Check that the object is some kind of JS object EXCEPT JS Value type.
  // In the case that the object is a value-wrapper object,
  // we enter the runtime system to make sure that indexing into string
  // objects work as intended.
  ASSERT(JS_OBJECT_TYPE > JS_VALUE_TYPE);
  __ ldrb(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
  __ cmp(r2, Operand(JS_OBJECT_TYPE));
  __ b(lt, &slow);

  // Get the elements array of the object.
  __ ldr(r1, FieldMemOperand(r1, JSObject::kElementsOffset));
  // Check that the object is in fast mode (not dictionary).
  __ ldr(r3, FieldMemOperand(r1, HeapObject::kMapOffset));
  __ cmp(r3, Operand(Factory::hash_table_map()));
  __ b(eq, &slow);
  // Check that the key (index) is within bounds.
  __ ldr(r3, FieldMemOperand(r1, Array::kLengthOffset));
  __ cmp(r0, Operand(r3));
  __ b(lo, &fast);

  // Slow case: Push extra copies of the arguments (2).
  __ bind(&slow);
  __ IncrementCounter(&Counters::keyed_load_generic_slow, 1, r0, r1);
  __ ldm(ia, sp, r0.bit() | r1.bit());
  __ stm(db_w, sp, r0.bit() | r1.bit());
  // Do tail-call to runtime routine.
  __ TailCallRuntime(ExternalReference(Runtime::kGetProperty), 2);

  // Fast case: Do the load.
  __ bind(&fast);
  __ add(r3, r1, Operand(Array::kHeaderSize - kHeapObjectTag));
  __ ldr(r0, MemOperand(r3, r0, LSL, kPointerSizeLog2));
  __ cmp(r0, Operand(Factory::the_hole_value()));
  // In case the loaded value is the_hole we have to consult GetProperty
  // to ensure the prototype chain is searched.
  __ b(eq, &slow);

  __ Ret();
}


void KeyedStoreIC::Generate(MacroAssembler* masm,
                            const ExternalReference& f) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- lr     : return address
  //  -- sp[0]  : key
  //  -- sp[1]  : receiver

  __ ldm(ia, sp, r2.bit() | r3.bit());
  __ stm(db_w, sp, r0.bit() | r2.bit() | r3.bit());

  __ TailCallRuntime(f, 3);
}


void KeyedStoreIC::GenerateGeneric(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- lr     : return address
  //  -- sp[0]  : key
  //  -- sp[1]  : receiver
  Label slow, fast, array, extra, exit;
  // Get the key and the object from the stack.
  __ ldm(ia, sp, r1.bit() | r3.bit());  // r1 = key, r3 = receiver
  // Check that the key is a smi.
  __ tst(r1, Operand(kSmiTagMask));
  __ b(ne, &slow);
  // Check that the object isn't a smi.
  __ tst(r3, Operand(kSmiTagMask));
  __ b(eq, &slow);
  // Get the map of the object.
  __ ldr(r2, FieldMemOperand(r3, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.  We need
  // to do this because this generic stub does not perform map checks.
  __ ldrb(ip, FieldMemOperand(r2, Map::kBitFieldOffset));
  __ tst(ip, Operand(1 << Map::kIsAccessCheckNeeded));
  __ b(ne, &slow);
  // Check if the object is a JS array or not.
  __ ldrb(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
  __ cmp(r2, Operand(JS_ARRAY_TYPE));
  // r1 == key.
  __ b(eq, &array);
  // Check that the object is some kind of JS object.
  __ cmp(r2, Operand(FIRST_JS_OBJECT_TYPE));
  __ b(lt, &slow);


  // Object case: Check key against length in the elements array.
  __ ldr(r3, FieldMemOperand(r3, JSObject::kElementsOffset));
  // Check that the object is in fast mode (not dictionary).
  __ ldr(r2, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ cmp(r2, Operand(Factory::hash_table_map()));
  __ b(eq, &slow);
  // Untag the key (for checking against untagged length in the fixed array).
  __ mov(r1, Operand(r1, ASR, kSmiTagSize));
  // Compute address to store into and check array bounds.
  __ add(r2, r3, Operand(Array::kHeaderSize - kHeapObjectTag));
  __ add(r2, r2, Operand(r1, LSL, kPointerSizeLog2));
  __ ldr(ip, FieldMemOperand(r3, Array::kLengthOffset));
  __ cmp(r1, Operand(ip));
  __ b(lo, &fast);


  // Slow case: Push extra copies of the arguments (3).
  __ bind(&slow);
  __ ldm(ia, sp, r1.bit() | r3.bit());  // r0 == value, r1 == key, r3 == object
  __ stm(db_w, sp, r0.bit() | r1.bit() | r3.bit());
  // Do tail-call to runtime routine.
  __ TailCallRuntime(ExternalReference(Runtime::kSetProperty), 3);

  // Extra capacity case: Check if there is extra capacity to
  // perform the store and update the length. Used for adding one
  // element to the array by writing to array[array.length].
  // r0 == value, r1 == key, r2 == elements, r3 == object
  __ bind(&extra);
  __ b(ne, &slow);  // do not leave holes in the array
  __ mov(r1, Operand(r1, ASR, kSmiTagSize));  // untag
  __ ldr(ip, FieldMemOperand(r2, Array::kLengthOffset));
  __ cmp(r1, Operand(ip));
  __ b(hs, &slow);
  __ mov(r1, Operand(r1, LSL, kSmiTagSize));  // restore tag
  __ add(r1, r1, Operand(1 << kSmiTagSize));  // and increment
  __ str(r1, FieldMemOperand(r3, JSArray::kLengthOffset));
  __ mov(r3, Operand(r2));
  // NOTE: Computing the address to store into must take the fact
  // that the key has been incremented into account.
  int displacement = Array::kHeaderSize - kHeapObjectTag -
      ((1 << kSmiTagSize) * 2);
  __ add(r2, r2, Operand(displacement));
  __ add(r2, r2, Operand(r1, LSL, kPointerSizeLog2 - kSmiTagSize));
  __ b(&fast);


  // Array case: Get the length and the elements array from the JS
  // array. Check that the array is in fast mode; if it is the
  // length is always a smi.
  // r0 == value, r3 == object
  __ bind(&array);
  __ ldr(r2, FieldMemOperand(r3, JSObject::kElementsOffset));
  __ ldr(r1, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ cmp(r1, Operand(Factory::hash_table_map()));
  __ b(eq, &slow);

  // Check the key against the length in the array, compute the
  // address to store into and fall through to fast case.
  __ ldr(r1, MemOperand(sp));  // restore key
  // r0 == value, r1 == key, r2 == elements, r3 == object.
  __ ldr(ip, FieldMemOperand(r3, JSArray::kLengthOffset));
  __ cmp(r1, Operand(ip));
  __ b(hs, &extra);
  __ mov(r3, Operand(r2));
  __ add(r2, r2, Operand(Array::kHeaderSize - kHeapObjectTag));
  __ add(r2, r2, Operand(r1, LSL, kPointerSizeLog2 - kSmiTagSize));


  // Fast case: Do the store.
  // r0 == value, r2 == address to store into, r3 == elements
  __ bind(&fast);
  __ str(r0, MemOperand(r2));
  // Skip write barrier if the written value is a smi.
  __ tst(r0, Operand(kSmiTagMask));
  __ b(eq, &exit);
  // Update write barrier for the elements array address.
  __ sub(r1, r2, Operand(r3));
  __ RecordWrite(r3, r1, r2);

  __ bind(&exit);
  __ Ret();
}


void KeyedStoreIC::GenerateExtendStorage(MacroAssembler* masm) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- lr     : return address
  //  -- sp[0]  : key
  //  -- sp[1]  : receiver
  // ----------- S t a t e -------------

  __ ldm(ia, sp, r2.bit() | r3.bit());
  __ stm(db_w, sp, r0.bit() | r2.bit() | r3.bit());

  // Perform tail call to the entry.
  __ TailCallRuntime(
      ExternalReference(IC_Utility(kSharedStoreIC_ExtendStorage)), 3);
}


void StoreIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------

  // Get the receiver from the stack and probe the stub cache.
  __ ldr(r1, MemOperand(sp));
  Code::Flags flags = Code::ComputeFlags(Code::STORE_IC,
                                         NOT_IN_LOOP,
                                         MONOMORPHIC);
  StubCache::GenerateProbe(masm, flags, r1, r2, r3);

  // Cache miss: Jump to runtime.
  Generate(masm, ExternalReference(IC_Utility(kStoreIC_Miss)));
}


void StoreIC::GenerateExtendStorage(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------

  __ ldr(r3, MemOperand(sp));  // copy receiver
  __ stm(db_w, sp, r0.bit() | r2.bit() | r3.bit());

  // Perform tail call to the entry.
  __ TailCallRuntime(
      ExternalReference(IC_Utility(kSharedStoreIC_ExtendStorage)), 3);
}


void StoreIC::Generate(MacroAssembler* masm, const ExternalReference& f) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r2    : name
  //  -- lr    : return address
  //  -- [sp]  : receiver
  // -----------------------------------

  __ ldr(r3, MemOperand(sp));  // copy receiver
  __ stm(db_w, sp, r0.bit() | r2.bit() | r3.bit());

  // Perform tail call to the entry.
  __ TailCallRuntime(f, 3);
}


#undef __


} }  // namespace v8::internal
