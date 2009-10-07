// Copyright 2009 the V8 project authors. All rights reserved.
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
// This function may return false negatives, so miss_label
// must always call a backup property load that is complete.
// This function is safe to call if the receiver has fast properties,
// or if name is not a symbol, and will jump to the miss_label in that case.
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
  // name - holds the name of the property and is unchanged.

  Label done;

  // Check for the absence of an interceptor.
  // Load the map into r0.
  __ movq(r0, FieldOperand(r1, JSObject::kMapOffset));
  // Test the has_named_interceptor bit in the map.
  __ testl(FieldOperand(r0, Map::kInstanceAttributesOffset),
          Immediate(1 << (Map::kHasNamedInterceptor + (3 * 8))));

  // Jump to miss if the interceptor bit is set.
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

  // Check that the properties array is a dictionary.
  __ movq(r0, FieldOperand(r1, JSObject::kPropertiesOffset));
  __ Cmp(FieldOperand(r0, HeapObject::kMapOffset), Factory::hash_table_map());
  __ j(not_equal, miss_label);

  // Compute the capacity mask.
  const int kCapacityOffset =
      StringDictionary::kHeaderSize +
      StringDictionary::kCapacityIndex * kPointerSize;
  __ movq(r2, FieldOperand(r0, kCapacityOffset));
  __ SmiToInteger32(r2, r2);
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
    __ movl(r1, FieldOperand(name, String::kLengthOffset));
    __ shrl(r1, Immediate(String::kHashShift));
    if (i > 0) {
      __ addl(r1, Immediate(StringDictionary::GetProbeOffset(i)));
    }
    __ and_(r1, r2);

    // Scale the index by multiplying by the entry size.
    ASSERT(StringDictionary::kEntrySize == 3);
    __ lea(r1, Operand(r1, r1, times_2, 0));  // r1 = r1 * 3

    // Check if the key is identical to the name.
    __ cmpq(name, Operand(r0, r1, times_pointer_size,
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
  __ testl(Operand(r0, r1, times_pointer_size, kDetailsOffset - kHeapObjectTag),
           Immediate(Smi::FromInt(PropertyDetails::TypeField::mask())));
  __ j(not_zero, miss_label);

  // Get the value at the masked, scaled index.
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ movq(r1,
          Operand(r0, r1, times_pointer_size, kValueOffset - kHeapObjectTag));
}


// Helper function used to check that a value is either not an object
// or is loaded if it is an object.
static void GenerateCheckNonObjectOrLoaded(MacroAssembler* masm, Label* miss,
                                           Register value) {
  Label done;
  // Check if the value is a Smi.
  __ JumpIfSmi(value, &done);
  // Check if the object has been loaded.
  __ movq(kScratchRegister, FieldOperand(value, JSFunction::kMapOffset));
  __ testb(FieldOperand(kScratchRegister, Map::kBitField2Offset),
           Immediate(1 << Map::kNeedsLoading));
  __ j(not_zero, miss);
  __ bind(&done);
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


void KeyedLoadIC::Generate(MacroAssembler* masm,
                           ExternalReference const& f) {
  // ----------- S t a t e -------------
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : name
  //  -- rsp[16] : receiver
  // -----------------------------------

  __ movq(rax, Operand(rsp, kPointerSize));
  __ movq(rcx, Operand(rsp, 2 * kPointerSize));
  __ pop(rbx);
  __ push(rcx);  // receiver
  __ push(rax);  // name
  __ push(rbx);  // return address

  // Perform tail call to the entry.
  __ TailCallRuntime(f, 2, 1);
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
  //  -- rsp[0] : return address
  //  -- rsp[8] : name
  //  -- rsp[16] : receiver
  // -----------------------------------
  Label slow, check_string, index_int, index_string, check_pixel_array;

  // Load name and receiver.
  __ movq(rax, Operand(rsp, kPointerSize));
  __ movq(rcx, Operand(rsp, 2 * kPointerSize));

  // Check that the object isn't a smi.
  __ JumpIfSmi(rcx, &slow);

  // Check that the object is some kind of JS object EXCEPT JS Value type.
  // In the case that the object is a value-wrapper object,
  // we enter the runtime system to make sure that indexing
  // into string objects work as intended.
  ASSERT(JS_OBJECT_TYPE > JS_VALUE_TYPE);
  __ CmpObjectType(rcx, JS_OBJECT_TYPE, rdx);
  __ j(below, &slow);
  // Check that the receiver does not require access checks.  We need
  // to check this explicitly since this generic stub does not perform
  // map checks.  The map is already in rdx.
  __ testb(FieldOperand(rdx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &slow);

  // Check that the key is a smi.
  __ JumpIfNotSmi(rax, &check_string);
  __ SmiToInteger32(rax, rax);
  // Get the elements array of the object.
  __ bind(&index_int);
  __ movq(rcx, FieldOperand(rcx, JSObject::kElementsOffset));
  // Check that the object is in fast mode (not dictionary).
  __ CompareRoot(FieldOperand(rcx, HeapObject::kMapOffset),
                 Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &check_pixel_array);
  // Check that the key (index) is within bounds.
  __ cmpl(rax, FieldOperand(rcx, FixedArray::kLengthOffset));
  __ j(above_equal, &slow);  // Unsigned comparison rejects negative indices.
  // Fast case: Do the load.
  __ movq(rax, Operand(rcx, rax, times_pointer_size,
                      FixedArray::kHeaderSize - kHeapObjectTag));
  __ CompareRoot(rax, Heap::kTheHoleValueRootIndex);
  // In case the loaded value is the_hole we have to consult GetProperty
  // to ensure the prototype chain is searched.
  __ j(equal, &slow);
  __ IncrementCounter(&Counters::keyed_load_generic_smi, 1);
  __ ret(0);

  // Check whether the elements is a pixel array.
  // rax: untagged index
  // rcx: elements array
  __ bind(&check_pixel_array);
  __ CompareRoot(FieldOperand(rcx, HeapObject::kMapOffset),
                 Heap::kPixelArrayMapRootIndex);
  __ j(not_equal, &slow);
  __ cmpl(rax, FieldOperand(rcx, PixelArray::kLengthOffset));
  __ j(above_equal, &slow);
  __ movq(rcx, FieldOperand(rcx, PixelArray::kExternalPointerOffset));
  __ movb(rax, Operand(rcx, rax, times_1, 0));
  __ Integer32ToSmi(rax, rax);
  __ ret(0);

  // Slow case: Load name and receiver from stack and jump to runtime.
  __ bind(&slow);
  __ IncrementCounter(&Counters::keyed_load_generic_slow, 1);
  KeyedLoadIC::Generate(masm, ExternalReference(Runtime::kKeyedGetProperty));
  __ bind(&check_string);
  // The key is not a smi.
  // Is it a string?
  __ CmpObjectType(rax, FIRST_NONSTRING_TYPE, rdx);
  __ j(above_equal, &slow);
  // Is the string an array index, with cached numeric value?
  __ movl(rbx, FieldOperand(rax, String::kLengthOffset));
  __ testl(rbx, Immediate(String::kIsArrayIndexMask));

  // If the string is a symbol, do a quick inline probe of the receiver's
  // dictionary, if it exists.
  __ j(not_zero, &index_string);  // The value in rbx is used at jump target.
  __ testb(FieldOperand(rdx, Map::kInstanceTypeOffset),
           Immediate(kIsSymbolMask));
  __ j(zero, &slow);
  // Probe the dictionary leaving result in ecx.
  GenerateDictionaryLoad(masm, &slow, rbx, rcx, rdx, rax);
  GenerateCheckNonObjectOrLoaded(masm, &slow, rcx);
  __ movq(rax, rcx);
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
  __ cmpl(rbx, Immediate(kLengthFieldLimit));
  __ j(above_equal, &slow);
  __ movl(rax, rbx);
  __ and_(rax, Immediate((1 << String::kShortLengthShift) - 1));
  __ shrl(rax, Immediate(String::kLongLengthShift));
  __ jmp(&index_int);
}


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rsp[0] : return address
  //  -- rsp[8] : name
  //  -- rsp[16] : receiver
  // -----------------------------------
  Generate(masm, ExternalReference(IC_Utility(kKeyedLoadIC_Miss)));
}


void KeyedStoreIC::Generate(MacroAssembler* masm, ExternalReference const& f) {
  // ----------- S t a t e -------------
  //  -- rax     : value
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : key
  //  -- rsp[16] : receiver
  // -----------------------------------

  __ pop(rcx);
  __ push(Operand(rsp, 1 * kPointerSize));  // receiver
  __ push(Operand(rsp, 1 * kPointerSize));  // key
  __ push(rax);  // value
  __ push(rcx);  // return address

  // Do tail-call to runtime routine.
  __ TailCallRuntime(f, 3, 1);
}


void KeyedStoreIC::GenerateExtendStorage(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax     : value
  //  -- rcx     : transition map
  //  -- rsp[0]  : return address
  //  -- rsp[8]  : key
  //  -- rsp[16] : receiver
  // -----------------------------------

  __ pop(rbx);
  __ push(Operand(rsp, 1 * kPointerSize));  // receiver
  __ push(rcx);  // transition map
  __ push(rax);  // value
  __ push(rbx);  // return address

  // Do tail-call to runtime routine.
  __ TailCallRuntime(
      ExternalReference(IC_Utility(kSharedStoreIC_ExtendStorage)), 3, 1);
}


void KeyedStoreIC::GenerateGeneric(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rsp[0] : return address
  //  -- rsp[8] : key
  //  -- rsp[16] : receiver
  // -----------------------------------
  Label slow, fast, array, extra, check_pixel_array;

  // Get the receiver from the stack.
  __ movq(rdx, Operand(rsp, 2 * kPointerSize));  // 2 ~ return address, key
  // Check that the object isn't a smi.
  __ JumpIfSmi(rdx, &slow);
  // Get the map from the receiver.
  __ movq(rcx, FieldOperand(rdx, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.  We need
  // to do this because this generic stub does not perform map checks.
  __ testb(FieldOperand(rcx, Map::kBitFieldOffset),
           Immediate(1 << Map::kIsAccessCheckNeeded));
  __ j(not_zero, &slow);
  // Get the key from the stack.
  __ movq(rbx, Operand(rsp, 1 * kPointerSize));  // 1 ~ return address
  // Check that the key is a smi.
  __ JumpIfNotSmi(rbx, &slow);
  // If it is a smi, make sure it is zero-extended, so it can be
  // used as an index in a memory operand.
  __ movl(rbx, rbx);  // Clear the high bits of rbx.

  __ CmpInstanceType(rcx, JS_ARRAY_TYPE);
  __ j(equal, &array);
  // Check that the object is some kind of JS object.
  __ CmpInstanceType(rcx, FIRST_JS_OBJECT_TYPE);
  __ j(below, &slow);

  // Object case: Check key against length in the elements array.
  // rax: value
  // rdx: JSObject
  // rbx: index (as a smi), zero-extended.
  __ movq(rcx, FieldOperand(rdx, JSObject::kElementsOffset));
  // Check that the object is in fast mode (not dictionary).
  __ CompareRoot(FieldOperand(rcx, HeapObject::kMapOffset),
                 Heap::kFixedArrayMapRootIndex);
  __ j(not_equal, &check_pixel_array);
  // Untag the key (for checking against untagged length in the fixed array).
  __ SmiToInteger32(rdx, rbx);
  __ cmpl(rdx, FieldOperand(rcx, Array::kLengthOffset));
  // rax: value
  // rcx: FixedArray
  // rbx: index (as a smi)
  __ j(below, &fast);

  // Slow case: Push extra copies of the arguments (3).
  __ bind(&slow);
  __ pop(rcx);
  __ push(Operand(rsp, 1 * kPointerSize));
  __ push(Operand(rsp, 1 * kPointerSize));
  __ push(rax);
  __ push(rcx);
  // Do tail-call to runtime routine.
  __ TailCallRuntime(ExternalReference(Runtime::kSetProperty), 3, 1);

  // Check whether the elements is a pixel array.
  // rax: value
  // rcx: elements array
  // rbx: index (as a smi), zero-extended.
  __ bind(&check_pixel_array);
  __ CompareRoot(FieldOperand(rcx, HeapObject::kMapOffset),
                 Heap::kPixelArrayMapRootIndex);
  __ j(not_equal, &slow);
  // Check that the value is a smi. If a conversion is needed call into the
  // runtime to convert and clamp.
  __ JumpIfNotSmi(rax, &slow);
  __ SmiToInteger32(rbx, rbx);
  __ cmpl(rbx, FieldOperand(rcx, PixelArray::kLengthOffset));
  __ j(above_equal, &slow);
  __ movq(rdx, rax);  // Save the value.
  __ SmiToInteger32(rax, rax);
  {  // Clamp the value to [0..255].
    Label done, is_negative;
    __ testl(rax, Immediate(0xFFFFFF00));
    __ j(zero, &done);
    __ j(negative, &is_negative);
    __ movl(rax, Immediate(255));
    __ jmp(&done);
    __ bind(&is_negative);
    __ xorl(rax, rax);  // Clear rax.
    __ bind(&done);
  }
  __ movq(rcx, FieldOperand(rcx, PixelArray::kExternalPointerOffset));
  __ movb(Operand(rcx, rbx, times_1, 0), rax);
  __ movq(rax, rdx);  // Return the original value.
  __ ret(0);

  // Extra capacity case: Check if there is extra capacity to
  // perform the store and update the length. Used for adding one
  // element to the array by writing to array[array.length].
  __ bind(&extra);
  // rax: value
  // rdx: JSArray
  // rcx: FixedArray
  // rbx: index (as a smi)
  // flags: compare (rbx, rdx.length())
  __ j(not_equal, &slow);  // do not leave holes in the array
  __ SmiToInteger64(rbx, rbx);
  __ cmpl(rbx, FieldOperand(rcx, FixedArray::kLengthOffset));
  __ j(above_equal, &slow);
  // Increment and restore smi-tag.
  __ Integer64AddToSmi(rbx, rbx, 1);
  __ movq(FieldOperand(rdx, JSArray::kLengthOffset), rbx);
  __ SmiSubConstant(rbx, rbx, 1, NULL);
  __ jmp(&fast);

  // Array case: Get the length and the elements array from the JS
  // array. Check that the array is in fast mode; if it is the
  // length is always a smi.
  __ bind(&array);
  // rax: value
  // rdx: JSArray
  // rbx: index (as a smi)
  __ movq(rcx, FieldOperand(rdx, JSObject::kElementsOffset));
  __ Cmp(FieldOperand(rcx, HeapObject::kMapOffset), Factory::fixed_array_map());
  __ j(not_equal, &slow);

  // Check the key against the length in the array, compute the
  // address to store into and fall through to fast case.
  __ cmpl(rbx, FieldOperand(rdx, JSArray::kLengthOffset));
  __ j(above_equal, &extra);

  // Fast case: Do the store.
  __ bind(&fast);
  // rax: value
  // rcx: FixedArray
  // rbx: index (as a smi)
  __ movq(Operand(rcx, rbx, times_half_pointer_size,
                  FixedArray::kHeaderSize - kHeapObjectTag),
         rax);
  // Update write barrier for the elements array address.
  __ movq(rdx, rax);
  __ RecordWrite(rcx, 0, rdx, rbx);
  __ ret(0);
}


void CallIC::Generate(MacroAssembler* masm,
                      int argc,
                      ExternalReference const& f) {
  // Get the receiver of the function from the stack; 1 ~ return address.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));
  // Get the name of the function to call from the stack.
  // 2 ~ receiver, return address.
  __ movq(rbx, Operand(rsp, (argc + 2) * kPointerSize));

  // Enter an internal frame.
  __ EnterInternalFrame();

  // Push the receiver and the name of the function.
  __ push(rdx);
  __ push(rbx);

  // Call the entry.
  CEntryStub stub(1);
  __ movq(rax, Immediate(2));
  __ movq(rbx, f);
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


// Defined in ic.cc.
Object* CallIC_Miss(Arguments args);

void CallIC::GenerateMegamorphic(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // rsp[0] return address
  // rsp[8] argument argc
  // rsp[16] argument argc - 1
  // ...
  // rsp[argc * 8] argument 1
  // rsp[(argc + 1) * 8] argument 0 = reciever
  // rsp[(argc + 2) * 8] function name
  // -----------------------------------
  Label number, non_number, non_string, boolean, probe, miss;

  // Get the receiver of the function from the stack; 1 ~ return address.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));
  // Get the name of the function from the stack; 2 ~ return address, receiver
  __ movq(rcx, Operand(rsp, (argc + 2) * kPointerSize));

  // Probe the stub cache.
  Code::Flags flags =
      Code::ComputeFlags(Code::CALL_IC, NOT_IN_LOOP, MONOMORPHIC, NORMAL, argc);
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

  // Cache miss: Jump to runtime.
  __ bind(&miss);
  Generate(masm, argc, ExternalReference(IC_Utility(kCallIC_Miss)));
}


static void GenerateNormalHelper(MacroAssembler* masm,
                                 int argc,
                                 bool is_global_object,
                                 Label* miss) {
  // Search dictionary - put result in register edx.
  GenerateDictionaryLoad(masm, miss, rax, rdx, rbx, rcx);

  // Move the result to register rdi and check that it isn't a smi.
  __ movq(rdi, rdx);
  __ JumpIfSmi(rdx, miss);

  // Check that the value is a JavaScript function.
  __ CmpObjectType(rdx, JS_FUNCTION_TYPE, rdx);
  __ j(not_equal, miss);
  // Check that the function has been loaded.
  __ testb(FieldOperand(rdx, Map::kBitField2Offset),
           Immediate(1 << Map::kNeedsLoading));
  __ j(not_zero, miss);

  // Patch the receiver with the global proxy if necessary.
  if (is_global_object) {
    __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));
    __ movq(rdx, FieldOperand(rdx, GlobalObject::kGlobalReceiverOffset));
    __ movq(Operand(rsp, (argc + 1) * kPointerSize), rdx);
  }

  // Invoke the function.
  ParameterCount actual(argc);
  __ InvokeFunction(rdi, actual, JUMP_FUNCTION);
}


void CallIC::GenerateNormal(MacroAssembler* masm, int argc) {
  // ----------- S t a t e -------------
  // rsp[0] return address
  // rsp[8] argument argc
  // rsp[16] argument argc - 1
  // ...
  // rsp[argc * 8] argument 1
  // rsp[(argc + 1) * 8] argument 0 = reciever
  // rsp[(argc + 2) * 8] function name
  // -----------------------------------

  Label miss, global_object, non_global_object;

  // Get the receiver of the function from the stack.
  __ movq(rdx, Operand(rsp, (argc + 1) * kPointerSize));
  // Get the name of the function from the stack.
  __ movq(rcx, Operand(rsp, (argc + 2) * kPointerSize));

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

  // Cache miss: Jump to runtime.
  __ bind(&miss);
  Generate(masm, argc, ExternalReference(IC_Utility(kCallIC_Miss)));
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


void LoadIC::Generate(MacroAssembler* masm, ExternalReference const& f) {
  // ----------- S t a t e -------------
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------

  __ movq(rax, Operand(rsp, kPointerSize));

  __ pop(rbx);
  __ push(rax);  // receiver
  __ push(rcx);  // name
  __ push(rbx);  // return address

  // Perform tail call to the entry.
  __ TailCallRuntime(f, 2, 1);
}


void LoadIC::GenerateArrayLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------

  Label miss;

  __ movq(rax, Operand(rsp, kPointerSize));

  StubCompiler::GenerateLoadArrayLength(masm, rax, rdx, &miss);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateFunctionPrototype(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------

  Label miss;

  __ movq(rax, Operand(rsp, kPointerSize));

  StubCompiler::GenerateLoadFunctionPrototype(masm, rax, rdx, rbx, &miss);
  __ bind(&miss);
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------

  __ movq(rax, Operand(rsp, kPointerSize));

  // Probe the stub cache.
  Code::Flags flags = Code::ComputeFlags(Code::LOAD_IC,
                                         NOT_IN_LOOP,
                                         MONOMORPHIC);
  StubCache::GenerateProbe(masm, flags, rax, rcx, rbx, rdx);

  // Cache miss: Jump to runtime.
  StubCompiler::GenerateLoadMiss(masm, Code::LOAD_IC);
}


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------

  Generate(masm, ExternalReference(IC_Utility(kLoadIC_Miss)));
}


void LoadIC::GenerateNormal(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------

  Label miss, probe, global;

  __ movq(rax, Operand(rsp, kPointerSize));

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

  // Search the dictionary placing the result in eax.
  __ bind(&probe);
  GenerateDictionaryLoad(masm, &miss, rdx, rax, rbx, rcx);
  GenerateCheckNonObjectOrLoaded(masm, &miss, rax);
  __ ret(0);

  // Global object access: Check access rights.
  __ bind(&global);
  __ CheckAccessGlobalProxy(rax, rdx, &miss);
  __ jmp(&probe);

  // Cache miss: Restore receiver from stack and jump to runtime.
  __ bind(&miss);
  __ movq(rax, Operand(rsp, 1 * kPointerSize));
  Generate(masm, ExternalReference(IC_Utility(kLoadIC_Miss)));
}


void LoadIC::GenerateStringLength(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------

  Label miss;

  __ movq(rax, Operand(rsp, kPointerSize));

  StubCompiler::GenerateLoadStringLength(masm, rax, rdx, &miss);
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

void StoreIC::Generate(MacroAssembler* masm, ExternalReference const& f) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------
  __ pop(rbx);
  __ push(Operand(rsp, 0));  // receiver
  __ push(rcx);  // name
  __ push(rax);  // value
  __ push(rbx);  // return address

  // Perform tail call to the entry.
  __ TailCallRuntime(f, 3, 1);
}

void StoreIC::GenerateExtendStorage(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : Map (target of map transition)
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------

  __ pop(rbx);
  __ push(Operand(rsp, 0));  // receiver
  __ push(rcx);  // transition map
  __ push(rax);  // value
  __ push(rbx);  // return address

  // Perform tail call to the entry.
  __ TailCallRuntime(
      ExternalReference(IC_Utility(kSharedStoreIC_ExtendStorage)), 3, 1);
}

void StoreIC::GenerateMegamorphic(MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rcx    : name
  //  -- rsp[0] : return address
  //  -- rsp[8] : receiver
  // -----------------------------------

  // Get the receiver from the stack and probe the stub cache.
  __ movq(rdx, Operand(rsp, kPointerSize));
  Code::Flags flags = Code::ComputeFlags(Code::STORE_IC,
                                         NOT_IN_LOOP,
                                         MONOMORPHIC);
  StubCache::GenerateProbe(masm, flags, rdx, rcx, rbx, no_reg);

  // Cache miss: Jump to runtime.
  Generate(masm, ExternalReference(IC_Utility(kStoreIC_Miss)));
}


#undef __


} }  // namespace v8::internal
