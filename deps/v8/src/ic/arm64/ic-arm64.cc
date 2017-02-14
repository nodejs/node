// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM64

#include "src/codegen.h"
#include "src/ic/ic.h"
#include "src/ic/ic-compiler.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {


#define __ ACCESS_MASM(masm)

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
static void GenerateDictionaryLoad(MacroAssembler* masm, Label* miss,
                                   Register elements, Register name,
                                   Register result, Register scratch1,
                                   Register scratch2) {
  DCHECK(!AreAliased(elements, name, scratch1, scratch2));
  DCHECK(!AreAliased(result, scratch1, scratch2));

  Label done;

  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm, miss, &done, elements,
                                                   name, scratch1, scratch2);

  // If probing finds an entry check that the value is a normal property.
  __ Bind(&done);

  static const int kElementsStartOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;
  static const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  __ Ldr(scratch1, FieldMemOperand(scratch2, kDetailsOffset));
  __ Tst(scratch1, Smi::FromInt(PropertyDetails::TypeField::kMask));
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
static void GenerateDictionaryStore(MacroAssembler* masm, Label* miss,
                                    Register elements, Register name,
                                    Register value, Register scratch1,
                                    Register scratch2) {
  DCHECK(!AreAliased(elements, name, value, scratch1, scratch2));

  Label done;

  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm, miss, &done, elements,
                                                   name, scratch1, scratch2);

  // If probing finds an entry in the dictionary check that the value
  // is a normal property that is not read only.
  __ Bind(&done);

  static const int kElementsStartOffset =
      NameDictionary::kHeaderSize +
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
  __ RecordWrite(elements, scratch2, scratch1, kLRHasNotBeenSaved,
                 kDontSaveFPRegs);
}

void LoadIC::GenerateNormal(MacroAssembler* masm) {
  Register dictionary = x0;
  DCHECK(!dictionary.is(LoadDescriptor::ReceiverRegister()));
  DCHECK(!dictionary.is(LoadDescriptor::NameRegister()));
  Label slow;

  __ Ldr(dictionary, FieldMemOperand(LoadDescriptor::ReceiverRegister(),
                                     JSObject::kPropertiesOffset));
  GenerateDictionaryLoad(masm, &slow, dictionary,
                         LoadDescriptor::NameRegister(), x0, x3, x4);
  __ Ret();

  // Dictionary load failed, go slow (but don't miss).
  __ Bind(&slow);
  GenerateRuntimeGetProperty(masm);
}


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // The return address is in lr.
  Isolate* isolate = masm->isolate();
  ASM_LOCATION("LoadIC::GenerateMiss");

  DCHECK(!AreAliased(x4, x5, LoadWithVectorDescriptor::SlotRegister(),
                     LoadWithVectorDescriptor::VectorRegister()));
  __ IncrementCounter(isolate->counters()->ic_load_miss(), 1, x4, x5);

  // Perform tail call to the entry.
  __ Push(LoadWithVectorDescriptor::ReceiverRegister(),
          LoadWithVectorDescriptor::NameRegister(),
          LoadWithVectorDescriptor::SlotRegister(),
          LoadWithVectorDescriptor::VectorRegister());
  __ TailCallRuntime(Runtime::kLoadIC_Miss);
}

void LoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // The return address is in lr.
  __ Push(LoadDescriptor::ReceiverRegister(), LoadDescriptor::NameRegister());

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kGetProperty);
}


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  // The return address is in lr.
  Isolate* isolate = masm->isolate();

  DCHECK(!AreAliased(x10, x11, LoadWithVectorDescriptor::SlotRegister(),
                     LoadWithVectorDescriptor::VectorRegister()));
  __ IncrementCounter(isolate->counters()->ic_keyed_load_miss(), 1, x10, x11);

  __ Push(LoadWithVectorDescriptor::ReceiverRegister(),
          LoadWithVectorDescriptor::NameRegister(),
          LoadWithVectorDescriptor::SlotRegister(),
          LoadWithVectorDescriptor::VectorRegister());

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kKeyedLoadIC_Miss);
}

void KeyedLoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // The return address is in lr.
  __ Push(LoadDescriptor::ReceiverRegister(), LoadDescriptor::NameRegister());

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kKeyedGetProperty);
}

static void StoreIC_PushArgs(MacroAssembler* masm) {
  __ Push(StoreWithVectorDescriptor::ValueRegister(),
          StoreWithVectorDescriptor::SlotRegister(),
          StoreWithVectorDescriptor::VectorRegister(),
          StoreWithVectorDescriptor::ReceiverRegister(),
          StoreWithVectorDescriptor::NameRegister());
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm) {
  ASM_LOCATION("KeyedStoreIC::GenerateMiss");
  StoreIC_PushArgs(masm);
  __ TailCallRuntime(Runtime::kKeyedStoreIC_Miss);
}

void KeyedStoreIC::GenerateSlow(MacroAssembler* masm) {
  ASM_LOCATION("KeyedStoreIC::GenerateSlow");
  StoreIC_PushArgs(masm);

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  __ TailCallRuntime(Runtime::kKeyedStoreIC_Slow);
}

static void KeyedStoreGenerateMegamorphicHelper(
    MacroAssembler* masm, Label* fast_object, Label* fast_double, Label* slow,
    KeyedStoreCheckMap check_map, KeyedStoreIncrementLength increment_length,
    Register value, Register key, Register receiver, Register receiver_map,
    Register elements_map, Register elements) {
  DCHECK(!AreAliased(value, key, receiver, receiver_map, elements_map, elements,
                     x10, x11));

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
    __ Add(x10, key, Smi::FromInt(1));
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
  __ RecordWrite(elements, address, x10, kLRHasNotBeenSaved, kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

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
  __ Add(x10, elements, FixedDoubleArray::kHeaderSize - kHeapObjectTag);
  __ Add(x10, x10, Operand::UntagSmiAndScale(key, kPointerSizeLog2));
  __ Ldr(x11, MemOperand(x10));
  __ CompareAndBranch(x11, kHoleNanInt64, ne, &fast_double_without_map_check);
  __ JumpIfDictionaryInPrototypeChain(receiver, elements_map, x10, slow);

  __ Bind(&fast_double_without_map_check);
  __ StoreNumberToDoubleElements(value, key, elements, x10, d0,
                                 &transition_double_elements);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ Add(x10, key, Smi::FromInt(1));
    __ Str(x10, FieldMemOperand(receiver, JSArray::kLengthOffset));
  }
  __ Ret();


  __ Bind(&transition_smi_elements);
  // Transition the array appropriately depending on the value type.
  __ Ldr(x10, FieldMemOperand(value, HeapObject::kMapOffset));
  __ JumpIfNotRoot(x10, Heap::kHeapNumberMapRootIndex, &non_double_value);

  // Value is a double. Transition FAST_SMI_ELEMENTS ->
  // FAST_DOUBLE_ELEMENTS and complete the store.
  __ LoadTransitionedArrayMapConditional(
      FAST_SMI_ELEMENTS, FAST_DOUBLE_ELEMENTS, receiver_map, x10, x11, slow);
  AllocationSiteMode mode =
      AllocationSite::GetMode(FAST_SMI_ELEMENTS, FAST_DOUBLE_ELEMENTS);
  ElementsTransitionGenerator::GenerateSmiToDouble(masm, receiver, key, value,
                                                   receiver_map, mode, slow);
  __ Ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ B(&fast_double_without_map_check);

  __ Bind(&non_double_value);
  // Value is not a double, FAST_SMI_ELEMENTS -> FAST_ELEMENTS.
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS, FAST_ELEMENTS,
                                         receiver_map, x10, x11, slow);

  mode = AllocationSite::GetMode(FAST_SMI_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
      masm, receiver, key, value, receiver_map, mode, slow);

  __ Ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ B(&finish_store);

  __ Bind(&transition_double_elements);
  // Elements are FAST_DOUBLE_ELEMENTS, but value is an Object that's not a
  // HeapNumber. Make sure that the receiver is a Array with FAST_ELEMENTS and
  // transition array from FAST_DOUBLE_ELEMENTS to FAST_ELEMENTS
  __ LoadTransitionedArrayMapConditional(FAST_DOUBLE_ELEMENTS, FAST_ELEMENTS,
                                         receiver_map, x10, x11, slow);
  mode = AllocationSite::GetMode(FAST_DOUBLE_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateDoubleToObject(
      masm, receiver, key, value, receiver_map, mode, slow);
  __ Ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ B(&finish_store);
}


void KeyedStoreIC::GenerateMegamorphic(MacroAssembler* masm,
                                       LanguageMode language_mode) {
  ASM_LOCATION("KeyedStoreIC::GenerateMegamorphic");
  Label slow;
  Label array;
  Label fast_object;
  Label extra;
  Label fast_object_grow;
  Label fast_double_grow;
  Label fast_double;
  Label maybe_name_key;
  Label miss;

  Register value = StoreDescriptor::ValueRegister();
  Register key = StoreDescriptor::NameRegister();
  Register receiver = StoreDescriptor::ReceiverRegister();
  DCHECK(receiver.is(x1));
  DCHECK(key.is(x2));
  DCHECK(value.is(x0));

  Register receiver_map = x3;
  Register elements = x4;
  Register elements_map = x5;

  __ JumpIfNotSmi(key, &maybe_name_key);
  __ JumpIfSmi(receiver, &slow);
  __ Ldr(receiver_map, FieldMemOperand(receiver, HeapObject::kMapOffset));

  // Check that the receiver does not require access checks.
  // The generic stub does not perform map checks.
  __ Ldrb(x10, FieldMemOperand(receiver_map, Map::kBitFieldOffset));
  __ TestAndBranchIfAnySet(x10, (1 << Map::kIsAccessCheckNeeded), &slow);

  // Check if the object is a JS array or not.
  Register instance_type = x10;
  __ CompareInstanceType(receiver_map, instance_type, JS_ARRAY_TYPE);
  __ B(eq, &array);
  // Check that the object is some kind of JS object EXCEPT JS Value type. In
  // the case that the object is a value-wrapper object, we enter the runtime
  // system to make sure that indexing into string objects works as intended.
  STATIC_ASSERT(JS_VALUE_TYPE < JS_OBJECT_TYPE);
  __ Cmp(instance_type, JS_OBJECT_TYPE);
  __ B(lo, &slow);

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
  PropertyICCompiler::GenerateRuntimeSetProperty(masm, language_mode);
  // Never returns to here.

  __ bind(&maybe_name_key);
  __ Ldr(x10, FieldMemOperand(key, HeapObject::kMapOffset));
  __ Ldrb(x10, FieldMemOperand(x10, Map::kInstanceTypeOffset));
  __ JumpIfNotUniqueNameInstanceType(x10, &slow);

  // The handlers in the stub cache expect a vector and slot. Since we won't
  // change the IC from any downstream misses, a dummy vector can be used.
  Register vector = StoreWithVectorDescriptor::VectorRegister();
  Register slot = StoreWithVectorDescriptor::SlotRegister();
  DCHECK(!AreAliased(vector, slot, x5, x6, x7, x8));
  Handle<TypeFeedbackVector> dummy_vector =
      TypeFeedbackVector::DummyVector(masm->isolate());
  int slot_index = dummy_vector->GetIndex(
      FeedbackVectorSlot(TypeFeedbackVector::kDummyKeyedStoreICSlot));
  __ LoadRoot(vector, Heap::kDummyVectorRootIndex);
  __ Mov(slot, Operand(Smi::FromInt(slot_index)));

  masm->isolate()->store_stub_cache()->GenerateProbe(masm, receiver, key, x5,
                                                     x6, x7, x8);
  // Cache miss.
  __ B(&miss);

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

  KeyedStoreGenerateMegamorphicHelper(
      masm, &fast_object, &fast_double, &slow, kCheckMap, kDontIncrementLength,
      value, key, receiver, receiver_map, elements_map, elements);
  KeyedStoreGenerateMegamorphicHelper(masm, &fast_object_grow,
                                      &fast_double_grow, &slow, kDontCheckMap,
                                      kIncrementLength, value, key, receiver,
                                      receiver_map, elements_map, elements);

  __ bind(&miss);
  GenerateMiss(masm);
}

void StoreIC::GenerateMiss(MacroAssembler* masm) {
  StoreIC_PushArgs(masm);

  // Tail call to the entry.
  __ TailCallRuntime(Runtime::kStoreIC_Miss);
}


void StoreIC::GenerateNormal(MacroAssembler* masm) {
  Label miss;
  Register value = StoreDescriptor::ValueRegister();
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  Register dictionary = x5;
  DCHECK(!AreAliased(value, receiver, name,
                     StoreWithVectorDescriptor::SlotRegister(),
                     StoreWithVectorDescriptor::VectorRegister(), x5, x6, x7));

  __ Ldr(dictionary, FieldMemOperand(receiver, JSObject::kPropertiesOffset));

  GenerateDictionaryStore(masm, &miss, dictionary, name, value, x6, x7);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->ic_store_normal_hit(), 1, x6, x7);
  __ Ret();

  // Cache miss: Jump to runtime.
  __ Bind(&miss);
  __ IncrementCounter(counters->ic_store_normal_miss(), 1, x6, x7);
  GenerateMiss(masm);
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
  Address info_address = Assembler::return_address_from_call_start(address);

  InstructionSequence* patch_info = InstructionSequence::At(info_address);
  return patch_info->IsInlineData();
}


// Activate a SMI fast-path by patching the instructions generated by
// JumpPatchSite::EmitJumpIf(Not)Smi(), using the information encoded by
// JumpPatchSite::EmitPatchInfo().
void PatchInlinedSmiCode(Isolate* isolate, Address address,
                         InlinedSmiCheck check) {
  // The patch information is encoded in the instruction stream using
  // instructions which have no side effects, so we can safely execute them.
  // The patch information is encoded directly after the call to the helper
  // function which is requesting this patch operation.
  Address info_address = Assembler::return_address_from_call_start(address);
  InlineSmiCheckInfo info(info_address);

  // Check and decode the patch information instruction.
  if (!info.HasSmiCheck()) {
    return;
  }

  if (FLAG_trace_ic) {
    PrintF("[  Patching ic at %p, marker=%p, SMI check=%p\n",
           static_cast<void*>(address), static_cast<void*>(info_address),
           static_cast<void*>(info.SmiCheck()));
  }

  // Patch and activate code generated by JumpPatchSite::EmitJumpIfNotSmi()
  // and JumpPatchSite::EmitJumpIfSmi().
  // Changing
  //   tb(n)z xzr, #0, <target>
  // to
  //   tb(!n)z test_reg, #0, <target>
  Instruction* to_patch = info.SmiCheck();
  PatchingAssembler patcher(isolate, to_patch, 1);
  DCHECK(to_patch->IsTestBranch());
  DCHECK(to_patch->ImmTestBranchBit5() == 0);
  DCHECK(to_patch->ImmTestBranchBit40() == 0);

  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagMask == 1);

  int branch_imm = to_patch->ImmTestBranch();
  Register smi_reg;
  if (check == ENABLE_INLINED_SMI_CHECK) {
    DCHECK(to_patch->Rt() == xzr.code());
    smi_reg = info.SmiRegister();
  } else {
    DCHECK(check == DISABLE_INLINED_SMI_CHECK);
    DCHECK(to_patch->Rt() != xzr.code());
    smi_reg = xzr;
  }

  if (to_patch->Mask(TestBranchMask) == TBZ) {
    // This is JumpIfNotSmi(smi_reg, branch_imm).
    patcher.tbnz(smi_reg, 0, branch_imm);
  } else {
    DCHECK(to_patch->Mask(TestBranchMask) == TBNZ);
    // This is JumpIfSmi(smi_reg, branch_imm).
    patcher.tbz(smi_reg, 0, branch_imm);
  }
}
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64
