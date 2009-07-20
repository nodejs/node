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


// Helper function used to load a property from a dictionary backing storage.
static void GenerateDictionaryLoad(MacroAssembler* masm, Label* miss_label,
                                   Register r0, Register r1, Register r2,
                                   Register name) {
  // Register use:
  //
  // r0   - used to hold the property dictionary.
  //
  // r1   - initially the receiver
  //      - used for the index into the property dictionary
  //      - holds the result on exit.
  //
  // r2   - used to hold the capacity of the property dictionary.
  //
  // name - holds the name of the property and is unchanges.

  Label done;

  // Check for the absence of an interceptor.
  // Load the map into r0.
  __ mov(r0, FieldOperand(r1, JSObject::kMapOffset));
  // Test the has_named_interceptor bit in the map.
  __ test(FieldOperand(r0, Map::kInstanceAttributesOffset),
          Immediate(1 << (Map::kHasNamedInterceptor + (3 * 8))));

  // Jump to miss if the interceptor bit is set.
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

  // Check that the properties array is a dictionary.
  __ mov(r0, FieldOperand(r1, JSObject::kPropertiesOffset));
  __ cmp(FieldOperand(r0, HeapObject::kMapOffset),
         Immediate(Factory::hash_table_map()));
  __ j(not_equal, miss_label);

  // Compute the capacity mask.
  const int kCapacityOffset =
      Array::kHeaderSize + StringDictionary::kCapacityIndex * kPointerSize;
  __ mov(r2, FieldOperand(r0, kCapacityOffset));
  __ shr(r2, kSmiTagSize);  // convert smi to int
  __ dec(r2);

  // Generate an unrolled loop that performs a few probes before
  // giving up. Measurements done on Gmail indicate that 2 probes
  // cover ~93% of loads from dictionaries.
  static const int kProbes = 4;
  const int kElementsStartOffset =
      Array::kHeaderSize + StringDictionary::kElementsStartIndex * kPointerSize;
  for (int i = 0; i < kProbes; i++) {
    // Compute the masked index: (hash + i + i * i) & mask.
    __ mov(r1, FieldOperand(name, String::kLengthOffset));
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


// Helper function used to check that a value is either not an object
// or is loaded if it is an object.
static void GenerateCheckNonObjectOrLoaded(MacroAssembler* masm, Label* miss,
                                           Register value, Register scratch) {
  Label done;
  // Check if the value is a Smi.
  __ test(value, Immediate(kSmiTagMask));
  __ j(zero, &done, not_taken);
  // Check if the object has been loaded.
  __ mov(scratch, FieldOperand(value, JSFunction::kMapOffset));
  __ mov(scratch, FieldOperand(scratch, Map::kBitField2Offset));
  __ test(scratch, Immediate(1 << Map::kNeedsLoading));
  __ j(not_zero, miss, not_taken);
  __ bind(&done);
}


const int LoadIC::kOffsetToLoadInstruction = 13;


void LoadIC::GenerateArrayLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------

  Label miss;

  __ mov(eax, Operand(esp, kPointerSize));

  StubCompiler::GenerateLoadArrayLength(masm, eax, edx, &miss);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateStringLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------

  Label miss;

  __ mov(eax, Operand(esp, kPointerSize));

  StubCompiler::GenerateLoadStringLength(masm, eax, edx, &miss);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateFunctionPrototype(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------

  Label miss;

  __ mov(eax, Operand(esp, kPointerSize));

  StubCompiler::GenerateLoadFunctionPrototype(masm, eax, edx, ebx, &miss);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


#ifdef DEBUG
// For use in assert below.
static int TenToThe(int exponent) {
  ASSERT(exponent <= 9);
  ASSERT(exponent >= 1);
  int answer = 10;
  for (int i = 1; i < exponent; i++) answer *= 10;
  return answer;
}
#endif


void KeyedLoadIC::GenerateGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- esp[0] : return address
  //  -- esp[4] : name
  //  -- esp[8] : receiver
  // -----------------------------------
  Label slow, fast, check_string, index_int, index_string;

  // Load name and receiver.
  __ mov(eax, (Operand(esp, kPointerSize)));
  __ mov(ecx, (Operand(esp, 2 * kPointerSize)));

  // Check that the object isn't a smi.
  __ test(ecx, Immediate(kSmiTagMask));
  __ j(zero, &slow, not_taken);

  // Get the map of the receiver.
  __ mov(edx, FieldOperand(ecx, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.  We need
  // to check this explicitly since this generic stub does not perform
  // map checks.
  __ movzx_b(ebx, FieldOperand(edx, Map::kBitFieldOffset));
  __ test(ebx, Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &slow, not_taken);
  // Check that the object is some kind of JS object EXCEPT JS Value type.
  // In the case that the object is a value-wrapper object,
  // we enter the runtime system to make sure that indexing
  // into string objects work as intended.
  ASSERT(JS_OBJECT_TYPE > JS_VALUE_TYPE);
  __ movzx_b(edx, FieldOperand(edx, Map::kInstanceTypeOffset));
  __ cmp(edx, JS_OBJECT_TYPE);
  __ j(less, &slow, not_taken);
  // Check that the key is a smi.
  __ test(eax, Immediate(kSmiTagMask));
  __ j(not_zero, &check_string, not_taken);
  __ sar(eax, kSmiTagSize);
  // Get the elements array of the object.
  __ bind(&index_int);
  __ mov(ecx, FieldOperand(ecx, JSObject::kElementsOffset));
  // Check that the object is in fast mode (not dictionary).
  __ cmp(FieldOperand(ecx, HeapObject::kMapOffset),
         Immediate(Factory::hash_table_map()));
  __ j(equal, &slow, not_taken);
  // Check that the key (index) is within bounds.
  __ cmp(eax, FieldOperand(ecx, Array::kLengthOffset));
  __ j(below, &fast, taken);
  // Slow case: Load name and receiver from stack and jump to runtime.
  __ bind(&slow);
  __ IncrementCounter(&Counters::keyed_load_generic_slow, 1);
  KeyedLoadIC::Generate(masm, ExternalReference(Runtime::kKeyedGetProperty));
  // Check if the key is a symbol that is not an array index.
  __ bind(&check_string);
  __ mov(ebx, FieldOperand(eax, String::kLengthOffset));
  __ test(ebx, Immediate(String::kIsArrayIndexMask));
  __ j(not_zero, &index_string, not_taken);
  __ mov(ebx, FieldOperand(eax, HeapObject::kMapOffset));
  __ movzx_b(ebx, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ test(ebx, Immediate(kIsSymbolMask));
  __ j(not_zero, &slow, not_taken);
  // Probe the dictionary leaving result in ecx.
  GenerateDictionaryLoad(masm, &slow, ebx, ecx, edx, eax);
  GenerateCheckNonObjectOrLoaded(masm, &slow, ecx, edx);
  __ mov(eax, Operand(ecx));
  __ IncrementCounter(&Counters::keyed_load_generic_symbol, 1);
  __ ret(0);
  // Array index string: If short enough use cache in length/hash field (ebx).
  // We assert that there are enough bits in an int32_t after the hash shift
  // bits have been subtracted to allow space for the length and the cached
  // array index.
  ASSERT(TenToThe(String::kMaxCachedArrayIndexLength) <
         (1 << (String::kShortLengthShift - String::kHashShift)));
  __ bind(&index_string);
  const int kLengthFieldLimit =
      (String::kMaxCachedArrayIndexLength + 1) << String::kShortLengthShift;
  __ cmp(ebx, kLengthFieldLimit);
  __ j(above_equal, &slow);
  __ mov(eax, Operand(ebx));
  __ and_(eax, (1 << String::kShortLengthShift) - 1);
  __ shr(eax, String::kLongLengthShift);
  __ jmp(&index_int);
  // Fast case: Do the load.
  __ bind(&fast);
  __ mov(eax, Operand(ecx, eax, times_4, Array::kHeaderSize - kHeapObjectTag));
  __ cmp(Operand(eax), Immediate(Factory::the_hole_value()));
  // In case the loaded value is the_hole we have to consult GetProperty
  // to ensure the prototype chain is searched.
  __ j(equal, &slow, not_taken);
  __ IncrementCounter(&Counters::keyed_load_generic_smi, 1);
  __ ret(0);
}


void KeyedStoreIC::GenerateGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- esp[0] : return address
  //  -- esp[4] : key
  //  -- esp[8] : receiver
  // -----------------------------------
  Label slow, fast, array, extra;

  // Get the receiver from the stack.
  __ mov(edx, Operand(esp, 2 * kPointerSize));  // 2 ~ return address, key
  // Check that the object isn't a smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &slow, not_taken);
  // Get the map from the receiver.
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.  We need
  // to do this because this generic stub does not perform map checks.
  __ movzx_b(ebx, FieldOperand(ecx, Map::kBitFieldOffset));
  __ test(ebx, Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &slow, not_taken);
  // Get the key from the stack.
  __ mov(ebx, Operand(esp, 1 * kPointerSize));  // 1 ~ return address
  // Check that the key is a smi.
  __ test(ebx, Immediate(kSmiTagMask));
  __ j(not_zero, &slow, not_taken);
  // Get the instance type from the map of the receiver.
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  // Check if the object is a JS array or not.
  __ cmp(ecx, JS_ARRAY_TYPE);
  __ j(equal, &array);
  // Check that the object is some kind of JS object.
  __ cmp(ecx, FIRST_JS_OBJECT_TYPE);
  __ j(less, &slow, not_taken);

  // Object case: Check key against length in the elements array.
  // eax: value
  // edx: JSObject
  // ebx: index (as a smi)
  __ mov(ecx, FieldOperand(edx, JSObject::kElementsOffset));
  // Check that the object is in fast mode (not dictionary).
  __ cmp(FieldOperand(ecx, HeapObject::kMapOffset),
         Immediate(Factory::hash_table_map()));
  __ j(equal, &slow, not_taken);
  // Untag the key (for checking against untagged length in the fixed array).
  __ mov(edx, Operand(ebx));
  __ sar(edx, kSmiTagSize);  // untag the index and use it for the comparison
  __ cmp(edx, FieldOperand(ecx, Array::kLengthOffset));
  // eax: value
  // ecx: FixedArray
  // ebx: index (as a smi)
  __ j(below, &fast, taken);


  // Slow case: Push extra copies of the arguments (3).
  __ bind(&slow);
  __ pop(ecx);
  __ push(Operand(esp, 1 * kPointerSize));
  __ push(Operand(esp, 1 * kPointerSize));
  __ push(eax);
  __ push(ecx);
  // Do tail-call to runtime routine.
  __ TailCallRuntime(ExternalReference(Runtime::kSetProperty), 3);


  // Extra capacity case: Check if there is extra capacity to
  // perform the store and update the length. Used for adding one
  // element to the array by writing to array[array.length].
  __ bind(&extra);
  // eax: value
  // edx: JSArray
  // ecx: FixedArray
  // ebx: index (as a smi)
  // flags: compare (ebx, edx.length())
  __ j(not_equal, &slow, not_taken);  // do not leave holes in the array
  __ sar(ebx, kSmiTagSize);  // untag
  __ cmp(ebx, FieldOperand(ecx, Array::kLengthOffset));
  __ j(above_equal, &slow, not_taken);
  // Restore tag and increment.
  __ lea(ebx, Operand(ebx, times_2, 1 << kSmiTagSize));
  __ mov(FieldOperand(edx, JSArray::kLengthOffset), ebx);
  __ sub(Operand(ebx), Immediate(1 << kSmiTagSize));  // decrement ebx again
  __ jmp(&fast);


  // Array case: Get the length and the elements array from the JS
  // array. Check that the array is in fast mode; if it is the
  // length is always a smi.
  __ bind(&array);
  // eax: value
  // edx: JSArray
  // ebx: index (as a smi)
  __ mov(ecx, FieldOperand(edx, JSObject::kElementsOffset));
  __ cmp(FieldOperand(ecx, HeapObject::kMapOffset),
         Immediate(Factory::hash_table_map()));
  __ j(equal, &slow, not_taken);

  // Check the key against the length in the array, compute the
  // address to store into and fall through to fast case.
  __ cmp(ebx, FieldOperand(edx, JSArray::kLengthOffset));
  __ j(above_equal, &extra, not_taken);


  // Fast case: Do the store.
  __ bind(&fast);
  // eax: value
  // ecx: FixedArray
  // ebx: index (as a smi)
  __ mov(Operand(ecx, ebx, times_2, Array::kHeaderSize - kHeapObjectTag), eax);
  // Update write barrier for the elements array address.
  __ mov(edx, Operand(eax));
  __ RecordWrite(ecx, 0, edx, ebx);
  __ ret(0);
}


// Defined in ic.cc.
Object* CallIC_Miss(Arguments args);

void CallIC::GenerateMegamorphic(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // -----------------------------------
  Label number, non_number, non_string, boolean, probe, miss;

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));
  // Get the name of the function from the stack; 2 ~ return address, receiver
  __ mov(ecx, Operand(esp, (argc + 2) * kPointerSize));

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
  Generate(masm, argc, ExternalReference(IC_Utility(kCallIC_Miss)));
}


static void GenerateNormalHelper(MacroAssembler* masm,
                                 int argc,
                                 bool is_global_object,
                                 Label* miss) {
  // Search dictionary - put result in register edx.
  GenerateDictionaryLoad(masm, miss, eax, edx, ebx, ecx);

  // Move the result to register edi and check that it isn't a smi.
  __ mov(edi, Operand(edx));
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, miss, not_taken);

  // Check that the value is a JavaScript function.
  __ CmpObjectType(edx, JS_FUNCTION_TYPE, edx);
  __ j(not_equal, miss, not_taken);

  // Check that the function has been loaded.
  __ mov(edx, FieldOperand(edi, JSFunction::kMapOffset));
  __ mov(edx, FieldOperand(edx, Map::kBitField2Offset));
  __ test(edx, Immediate(1 << Map::kNeedsLoading));
  __ j(not_zero, miss, not_taken);

  // Patch the receiver with the global proxy if necessary.
  if (is_global_object) {
    __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));
    __ mov(edx, FieldOperand(edx, GlobalObject::kGlobalReceiverOffset));
    __ mov(Operand(esp, (argc + 1) * kPointerSize), edx);
  }

  // Invoke the function.
  ParameterCount actual(argc);
  __ InvokeFunction(edi, actual, JUMP_FUNCTION);
}


void CallIC::GenerateNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // -----------------------------------

  Label miss, global_object, non_global_object;

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));
  // Get the name of the function from the stack; 2 ~ return address, receiver.
  __ mov(ecx, Operand(esp, (argc + 2) * kPointerSize));

  // Check that the receiver isn't a smi.
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &miss, not_taken);

  // Check that the receiver is a valid JS object.
  __ mov(ebx, FieldOperand(edx, HeapObject::kMapOffset));
  __ movzx_b(eax, FieldOperand(ebx, Map::kInstanceTypeOffset));
  __ cmp(eax, FIRST_JS_OBJECT_TYPE);
  __ j(less, &miss, not_taken);

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
  Generate(masm, argc, ExternalReference(IC_Utility(kCallIC_Miss)));
}


void CallIC::Generate(MacroAssembler* masm,
                      int argc,
                      const ExternalReference& f) {
  // ----------- S t a t e -------------
  // -----------------------------------

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));
  // Get the name of the function to call from the stack.
  // 2 ~ receiver, return address.
  __ mov(ebx, Operand(esp, (argc + 2) * kPointerSize));

  // Enter an internal frame.
  __ EnterInternalFrame();

  // Push the receiver and the name of the function.
  __ push(edx);
  __ push(ebx);

  // Call the entry.
  CEntryStub stub;
  __ mov(eax, Immediate(2));
  __ mov(ebx, Immediate(f));
  __ CallStub(&stub);

  // Move result to edi and exit the internal frame.
  __ mov(edi, eax);
  __ LeaveInternalFrame();

  // Check if the receiver is a global object of some sort.
  Label invoke, global;
  __ mov(edx, Operand(esp, (argc + 1) * kPointerSize));  // receiver
  __ test(edx, Immediate(kSmiTagMask));
  __ j(zero, &invoke, not_taken);
  __ mov(ecx, FieldOperand(edx, HeapObject::kMapOffset));
  __ movzx_b(ecx, FieldOperand(ecx, Map::kInstanceTypeOffset));
  __ cmp(ecx, JS_GLOBAL_OBJECT_TYPE);
  __ j(equal, &global);
  __ cmp(ecx, JS_BUILTINS_OBJECT_TYPE);
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
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------

  __ mov(eax, Operand(esp, kPointerSize));

  // Probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(Code::LOAD_IC,
                                         NOT_IN_LOOP,
                                         MONOMORPHIC);
  StubCache::GenerateProbe(masm, flags, eax, ecx, ebx, edx);

  // Cache miss: Jump to runtime.
  Generate(masm, ExternalReference(IC_Utility(kLoadIC_Miss)));
}


void LoadIC::GenerateNormal(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------

  Label miss, probe, global;

  __ mov(eax, Operand(esp, kPointerSize));

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
  GenerateDictionaryLoad(masm, &miss, edx, eax, ebx, ecx);
  GenerateCheckNonObjectOrLoaded(masm, &miss, eax, edx);
  __ ret(0);

  // Global object access: Check access rights.
  __ bind(&global);
  __ CheckAccessGlobalProxy(eax, edx, &miss);
  __ jmp(&probe);

  // Cache miss: Restore receiver from stack and jump to runtime.
  __ bind(&miss);
  __ mov(eax, Operand(esp, 1 * kPointerSize));
  Generate(masm, ExternalReference(IC_Utility(kLoadIC_Miss)));
}


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------

  Generate(masm, ExternalReference(IC_Utility(kLoadIC_Miss)));
}


void LoadIC::Generate(MacroAssembler* masm, const ExternalReference& f) {
  // ----------- S t a t e -------------
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------

  __ mov(eax, Operand(esp, kPointerSize));

  // Move the return address below the arguments.
  __ pop(ebx);
  __ push(eax);
  __ push(ecx);
  __ push(ebx);

  // Perform tail call to the entry.
  __ TailCallRuntime(f, 2);
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
  Address test_instruction_address = address + 4;
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
  Address test_instruction_address = address + 4;  // 4 = stub address
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
  //  -- esp[0] : return address
  //  -- esp[4] : name
  //  -- esp[8] : receiver
  // -----------------------------------

  Generate(masm, ExternalReference(IC_Utility(kKeyedLoadIC_Miss)));
}


void KeyedLoadIC::Generate(MacroAssembler* masm, const ExternalReference& f) {
  // ----------- S t a t e -------------
  //  -- esp[0] : return address
  //  -- esp[4] : name
  //  -- esp[8] : receiver
  // -----------------------------------

  __ mov(eax, Operand(esp, kPointerSize));
  __ mov(ecx, Operand(esp, 2 * kPointerSize));

  // Move the return address below the arguments.
  __ pop(ebx);
  __ push(ecx);
  __ push(eax);
  __ push(ebx);

  // Perform tail call to the entry.
  __ TailCallRuntime(f, 2);
}


void StoreIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------

  // Get the receiver from the stack and probe the stub cache.
  __ mov(edx, Operand(esp, 4));
  Code::Flags flags = Code::ComputeFlags(Code::STORE_IC,
                                         NOT_IN_LOOP,
                                         MONOMORPHIC);
  StubCache::GenerateProbe(masm, flags, edx, ecx, ebx, no_reg);

  // Cache miss: Jump to runtime.
  Generate(masm, ExternalReference(IC_Utility(kStoreIC_Miss)));
}


void StoreIC::GenerateExtendStorage(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : transition map
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------

  // Move the return address below the arguments.
  __ pop(ebx);
  __ push(Operand(esp, 0));
  __ push(ecx);
  __ push(eax);
  __ push(ebx);
  // Perform tail call to the entry.
  __ TailCallRuntime(
      ExternalReference(IC_Utility(kSharedStoreIC_ExtendStorage)), 3);
}


void StoreIC::Generate(MacroAssembler* masm, const ExternalReference& f) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : name
  //  -- esp[0] : return address
  //  -- esp[4] : receiver
  // -----------------------------------

  // Move the return address below the arguments.
  __ pop(ebx);
  __ push(Operand(esp, 0));
  __ push(ecx);
  __ push(eax);
  __ push(ebx);

  // Perform tail call to the entry.
  __ TailCallRuntime(f, 3);
}


// Defined in ic.cc.
Object* KeyedStoreIC_Miss(Arguments args);

void KeyedStoreIC::Generate(MacroAssembler* masm, const ExternalReference& f) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- esp[0] : return address
  //  -- esp[4] : key
  //  -- esp[8] : receiver
  // -----------------------------------

  // Move the return address below the arguments.
  __ pop(ecx);
  __ push(Operand(esp, 1 * kPointerSize));
  __ push(Operand(esp, 1 * kPointerSize));
  __ push(eax);
  __ push(ecx);

  // Do tail-call to runtime routine.
  __ TailCallRuntime(f, 3);
}


void KeyedStoreIC::GenerateExtendStorage(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- eax    : value
  //  -- ecx    : transition map
  //  -- esp[0] : return address
  //  -- esp[4] : key
  //  -- esp[8] : receiver
  // -----------------------------------

  // Move the return address below the arguments.
  __ pop(ebx);
  __ push(Operand(esp, 1 * kPointerSize));
  __ push(ecx);
  __ push(eax);
  __ push(ebx);

  // Do tail-call to runtime routine.
  __ TailCallRuntime(
      ExternalReference(IC_Utility(kSharedStoreIC_ExtendStorage)), 3);
}

#undef __


} }  // namespace v8::internal
