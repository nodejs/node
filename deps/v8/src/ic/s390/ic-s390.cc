// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include "src/ic/ic.h"
#include "src/codegen.h"
#include "src/ic/ic-compiler.h"
#include "src/ic/stub-cache.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Static IC stub generators.
//

#define __ ACCESS_MASM(masm)

static void GenerateGlobalInstanceTypeCheck(MacroAssembler* masm, Register type,
                                            Label* global_object) {
  // Register usage:
  //   type: holds the receiver instance type on entry.
  __ CmpP(type, Operand(JS_GLOBAL_OBJECT_TYPE));
  __ beq(global_object);
  __ CmpP(type, Operand(JS_GLOBAL_PROXY_TYPE));
  __ beq(global_object);
}

// Helper function used from LoadIC GenerateNormal.
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
static void GenerateDictionaryLoad(MacroAssembler* masm, Label* miss,
                                   Register elements, Register name,
                                   Register result, Register scratch1,
                                   Register scratch2) {
  // Main use of the scratch registers.
  // scratch1: Used as temporary and to hold the capacity of the property
  //           dictionary.
  // scratch2: Used as temporary.
  Label done;

  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm, miss, &done, elements,
                                                   name, scratch1, scratch2);

  // If probing finds an entry check that the value is a normal
  // property.
  __ bind(&done);  // scratch2 == elements + 4 * index
  const int kElementsStartOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;
  const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  __ LoadP(scratch1, FieldMemOperand(scratch2, kDetailsOffset));
  __ LoadRR(r0, scratch2);
  __ LoadSmiLiteral(scratch2, Smi::FromInt(PropertyDetails::TypeField::kMask));
  __ AndP(scratch2, scratch1);
  __ bne(miss);
  __ LoadRR(scratch2, r0);

  // Get the value at the masked, scaled index and return.
  __ LoadP(result,
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
static void GenerateDictionaryStore(MacroAssembler* masm, Label* miss,
                                    Register elements, Register name,
                                    Register value, Register scratch1,
                                    Register scratch2) {
  // Main use of the scratch registers.
  // scratch1: Used as temporary and to hold the capacity of the property
  //           dictionary.
  // scratch2: Used as temporary.
  Label done;

  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm, miss, &done, elements,
                                                   name, scratch1, scratch2);

  // If probing finds an entry in the dictionary check that the value
  // is a normal property that is not read only.
  __ bind(&done);  // scratch2 == elements + 4 * index
  const int kElementsStartOffset =
      NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;
  const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  int kTypeAndReadOnlyMask =
      PropertyDetails::TypeField::kMask |
      PropertyDetails::AttributesField::encode(READ_ONLY);
  __ LoadP(scratch1, FieldMemOperand(scratch2, kDetailsOffset));
  __ LoadRR(r0, scratch2);
  __ LoadSmiLiteral(scratch2, Smi::FromInt(kTypeAndReadOnlyMask));
  __ AndP(scratch2, scratch1);
  __ bne(miss /*, cr0*/);
  __ LoadRR(scratch2, r0);

  // Store the value at the masked, scaled index and return.
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ AddP(scratch2, Operand(kValueOffset - kHeapObjectTag));
  __ StoreP(value, MemOperand(scratch2));

  // Update the write barrier. Make sure not to clobber the value.
  __ LoadRR(scratch1, value);
  __ RecordWrite(elements, scratch2, scratch1, kLRHasNotBeenSaved,
                 kDontSaveFPRegs);
}

// Checks the receiver for special cases (value type, slow case bits).
// Falls through for regular JS object.
static void GenerateKeyedLoadReceiverCheck(MacroAssembler* masm,
                                           Register receiver, Register map,
                                           Register scratch,
                                           int interceptor_bit, Label* slow) {
  // Check that the object isn't a smi.
  __ JumpIfSmi(receiver, slow);
  // Get the map of the receiver.
  __ LoadP(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  // Check bit field.
  __ LoadlB(scratch, FieldMemOperand(map, Map::kBitFieldOffset));
  DCHECK(((1 << Map::kIsAccessCheckNeeded) | (1 << interceptor_bit)) < 0x8000);
  __ mov(r0,
         Operand((1 << Map::kIsAccessCheckNeeded) | (1 << interceptor_bit)));
  __ AndP(r0, scratch);
  __ bne(slow /*, cr0*/);
  // Check that the object is some kind of JS object EXCEPT JS Value type.
  // In the case that the object is a value-wrapper object,
  // we enter the runtime system to make sure that indexing into string
  // objects work as intended.
  DCHECK(JS_OBJECT_TYPE > JS_VALUE_TYPE);
  __ LoadlB(scratch, FieldMemOperand(map, Map::kInstanceTypeOffset));
  __ CmpP(scratch, Operand(JS_OBJECT_TYPE));
  __ blt(slow);
}

// Loads an indexed element from a fast case array.
static void GenerateFastArrayLoad(MacroAssembler* masm, Register receiver,
                                  Register key, Register elements,
                                  Register scratch1, Register scratch2,
                                  Register result, Label* slow) {
  // Register use:
  //
  // receiver - holds the receiver on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // key      - holds the smi key on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // result   - holds the result on exit if the load succeeded.
  //            Allowed to be the the same as 'receiver' or 'key'.
  //            Unchanged on bailout so 'receiver' and 'key' can be safely
  //            used by further computation.
  //
  // Scratch registers:
  //
  // elements - holds the elements of the receiver and its protoypes.
  //
  // scratch1 - used to hold elements length, bit fields, base addresses.
  //
  // scratch2 - used to hold maps, prototypes, and the loaded value.
  Label check_prototypes, check_next_prototype;
  Label done, in_bounds, absent;

  __ LoadP(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ AssertFastElements(elements);

  // Check that the key (index) is within bounds.
  __ LoadP(scratch1, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ CmpLogicalP(key, scratch1);
  __ blt(&in_bounds, Label::kNear);
  // Out-of-bounds. Check the prototype chain to see if we can just return
  // 'undefined'.
  __ CmpP(key, Operand::Zero());
  __ blt(slow);  // Negative keys can't take the fast OOB path.
  __ bind(&check_prototypes);
  __ LoadP(scratch2, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ bind(&check_next_prototype);
  __ LoadP(scratch2, FieldMemOperand(scratch2, Map::kPrototypeOffset));
  // scratch2: current prototype
  __ CompareRoot(scratch2, Heap::kNullValueRootIndex);
  __ beq(&absent, Label::kNear);
  __ LoadP(elements, FieldMemOperand(scratch2, JSObject::kElementsOffset));
  __ LoadP(scratch2, FieldMemOperand(scratch2, HeapObject::kMapOffset));
  // elements: elements of current prototype
  // scratch2: map of current prototype
  __ CompareInstanceType(scratch2, scratch1, JS_OBJECT_TYPE);
  __ blt(slow);
  __ LoadlB(scratch1, FieldMemOperand(scratch2, Map::kBitFieldOffset));
  __ AndP(r0, scratch1, Operand((1 << Map::kIsAccessCheckNeeded) |
                                (1 << Map::kHasIndexedInterceptor)));
  __ bne(slow);
  __ CompareRoot(elements, Heap::kEmptyFixedArrayRootIndex);
  __ bne(slow);
  __ jmp(&check_next_prototype);

  __ bind(&absent);
  __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
  __ jmp(&done);

  __ bind(&in_bounds);
  // Fast case: Do the load.
  __ AddP(scratch1, elements,
          Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  // The key is a smi.
  __ SmiToPtrArrayOffset(scratch2, key);
  __ LoadP(scratch2, MemOperand(scratch2, scratch1));
  __ CompareRoot(scratch2, Heap::kTheHoleValueRootIndex);
  // In case the loaded value is the_hole we have to check the prototype chain.
  __ beq(&check_prototypes);
  __ LoadRR(result, scratch2);
  __ bind(&done);
}

// Checks whether a key is an array index string or a unique name.
// Falls through if a key is a unique name.
static void GenerateKeyNameCheck(MacroAssembler* masm, Register key,
                                 Register map, Register hash,
                                 Label* index_string, Label* not_unique) {
  // The key is not a smi.
  Label unique;
  // Is it a name?
  __ CompareObjectType(key, map, hash, LAST_UNIQUE_NAME_TYPE);
  __ bgt(not_unique);
  STATIC_ASSERT(LAST_UNIQUE_NAME_TYPE == FIRST_NONSTRING_TYPE);
  __ beq(&unique, Label::kNear);

  // Is the string an array index, with cached numeric value?
  __ LoadlW(hash, FieldMemOperand(key, Name::kHashFieldOffset));
  __ mov(r7, Operand(Name::kContainsCachedArrayIndexMask));
  __ AndP(r0, hash, r7);
  __ beq(index_string);

  // Is the string internalized? We know it's a string, so a single
  // bit test is enough.
  // map: key map
  __ LoadlB(hash, FieldMemOperand(map, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0);
  __ tmll(hash, Operand(kIsNotInternalizedMask));
  __ bne(not_unique);

  __ bind(&unique);
}

void LoadIC::GenerateNormal(MacroAssembler* masm) {
  Register dictionary = r2;
  DCHECK(!dictionary.is(LoadDescriptor::ReceiverRegister()));
  DCHECK(!dictionary.is(LoadDescriptor::NameRegister()));

  Label slow;

  __ LoadP(dictionary, FieldMemOperand(LoadDescriptor::ReceiverRegister(),
                                       JSObject::kPropertiesOffset));
  GenerateDictionaryLoad(masm, &slow, dictionary,
                         LoadDescriptor::NameRegister(), r2, r5, r6);
  __ Ret();

  // Dictionary load failed, go slow (but don't miss).
  __ bind(&slow);
  GenerateRuntimeGetProperty(masm);
}

// A register that isn't one of the parameters to the load ic.
static const Register LoadIC_TempRegister() { return r5; }

static void LoadIC_PushArgs(MacroAssembler* masm) {
  Register receiver = LoadDescriptor::ReceiverRegister();
  Register name = LoadDescriptor::NameRegister();
  Register slot = LoadDescriptor::SlotRegister();
  Register vector = LoadWithVectorDescriptor::VectorRegister();

  __ Push(receiver, name, slot, vector);
}

void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // The return address is in lr.
  Isolate* isolate = masm->isolate();

  DCHECK(!AreAliased(r6, r7, LoadWithVectorDescriptor::SlotRegister(),
                     LoadWithVectorDescriptor::VectorRegister()));
  __ IncrementCounter(isolate->counters()->ic_load_miss(), 1, r6, r7);

  LoadIC_PushArgs(masm);

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kLoadIC_Miss);
}

void LoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // The return address is in lr.

  __ LoadRR(LoadIC_TempRegister(), LoadDescriptor::ReceiverRegister());
  __ Push(LoadIC_TempRegister(), LoadDescriptor::NameRegister());

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kGetProperty);
}

void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  // The return address is in lr.
  Isolate* isolate = masm->isolate();

  DCHECK(!AreAliased(r6, r7, LoadWithVectorDescriptor::SlotRegister(),
                     LoadWithVectorDescriptor::VectorRegister()));
  __ IncrementCounter(isolate->counters()->ic_keyed_load_miss(), 1, r6, r7);

  LoadIC_PushArgs(masm);

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kKeyedLoadIC_Miss);
}

void KeyedLoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // The return address is in lr.

  __ Push(LoadDescriptor::ReceiverRegister(), LoadDescriptor::NameRegister());

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kKeyedGetProperty);
}

void KeyedLoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  // The return address is in lr.
  Label slow, check_name, index_smi, index_name, property_array_property;
  Label probe_dictionary, check_number_dictionary;

  Register key = LoadDescriptor::NameRegister();
  Register receiver = LoadDescriptor::ReceiverRegister();
  DCHECK(key.is(r4));
  DCHECK(receiver.is(r3));

  Isolate* isolate = masm->isolate();

  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &check_name);
  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from below
  // where a numeric string is converted to a smi.

  GenerateKeyedLoadReceiverCheck(masm, receiver, r2, r5,
                                 Map::kHasIndexedInterceptor, &slow);

  // Check the receiver's map to see if it has fast elements.
  __ CheckFastElements(r2, r5, &check_number_dictionary);

  GenerateFastArrayLoad(masm, receiver, key, r2, r5, r6, r2, &slow);
  __ IncrementCounter(isolate->counters()->ic_keyed_load_generic_smi(), 1, r6,
                      r5);
  __ Ret();

  __ bind(&check_number_dictionary);
  __ LoadP(r6, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ LoadP(r5, FieldMemOperand(r6, JSObject::kMapOffset));

  // Check whether the elements is a number dictionary.
  // r5: elements map
  // r6: elements
  __ CompareRoot(r5, Heap::kHashTableMapRootIndex);
  __ bne(&slow, Label::kNear);
  __ SmiUntag(r2, key);
  __ LoadFromNumberDictionary(&slow, r6, key, r2, r2, r5, r7);
  __ Ret();

  // Slow case, key and receiver still in r2 and r3.
  __ bind(&slow);
  __ IncrementCounter(isolate->counters()->ic_keyed_load_generic_slow(), 1, r6,
                      r5);
  GenerateRuntimeGetProperty(masm);

  __ bind(&check_name);
  GenerateKeyNameCheck(masm, key, r2, r5, &index_name, &slow);

  GenerateKeyedLoadReceiverCheck(masm, receiver, r2, r5,
                                 Map::kHasNamedInterceptor, &slow);

  // If the receiver is a fast-case object, check the stub cache. Otherwise
  // probe the dictionary.
  __ LoadP(r5, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ LoadP(r6, FieldMemOperand(r5, HeapObject::kMapOffset));
  __ CompareRoot(r6, Heap::kHashTableMapRootIndex);
  __ beq(&probe_dictionary);

  // The handlers in the stub cache expect a vector and slot. Since we won't
  // change the IC from any downstream misses, a dummy vector can be used.
  Register vector = LoadWithVectorDescriptor::VectorRegister();
  Register slot = LoadWithVectorDescriptor::SlotRegister();
  DCHECK(!AreAliased(vector, slot, r6, r7, r8, r9));
  Handle<TypeFeedbackVector> dummy_vector =
      TypeFeedbackVector::DummyVector(masm->isolate());
  int slot_index = dummy_vector->GetIndex(
      FeedbackVectorSlot(TypeFeedbackVector::kDummyKeyedLoadICSlot));
  __ LoadRoot(vector, Heap::kDummyVectorRootIndex);
  __ LoadSmiLiteral(slot, Smi::FromInt(slot_index));

  masm->isolate()->load_stub_cache()->GenerateProbe(masm, receiver, key, r6, r7,
                                                    r8, r9);
  // Cache miss.
  GenerateMiss(masm);

  // Do a quick inline probe of the receiver's dictionary, if it
  // exists.
  __ bind(&probe_dictionary);
  // r5: elements
  __ LoadP(r2, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ LoadlB(r2, FieldMemOperand(r2, Map::kInstanceTypeOffset));
  GenerateGlobalInstanceTypeCheck(masm, r2, &slow);
  // Load the property to r2.
  GenerateDictionaryLoad(masm, &slow, r5, key, r2, r7, r6);
  __ IncrementCounter(isolate->counters()->ic_keyed_load_generic_symbol(), 1,
                      r6, r5);
  __ Ret();

  __ bind(&index_name);
  __ IndexFromHash(r5, key);
  // Now jump to the place where smi keys are handled.
  __ b(&index_smi);
}

static void StoreIC_PushArgs(MacroAssembler* masm) {
  __ Push(StoreWithVectorDescriptor::ValueRegister(),
          StoreWithVectorDescriptor::SlotRegister(),
          StoreWithVectorDescriptor::VectorRegister(),
          StoreWithVectorDescriptor::ReceiverRegister(),
          StoreWithVectorDescriptor::NameRegister());
}

void KeyedStoreIC::GenerateMiss(MacroAssembler* masm) {
  StoreIC_PushArgs(masm);

  __ TailCallRuntime(Runtime::kKeyedStoreIC_Miss);
}

void KeyedStoreIC::GenerateSlow(MacroAssembler* masm) {
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
  Label transition_smi_elements;
  Label finish_object_store, non_double_value, transition_double_elements;
  Label fast_double_without_map_check;

  // Fast case: Do the store, could be either Object or double.
  __ bind(fast_object);
  Register scratch = r6;
  Register address = r7;
  DCHECK(!AreAliased(value, key, receiver, receiver_map, elements_map, elements,
                     scratch, address));

  if (check_map == kCheckMap) {
    __ LoadP(elements_map, FieldMemOperand(elements, HeapObject::kMapOffset));
    __ CmpP(elements_map,
            Operand(masm->isolate()->factory()->fixed_array_map()));
    __ bne(fast_double);
  }

  // HOLECHECK: guards "A[i] = V"
  // We have to go to the runtime if the current value is the hole because
  // there may be a callback on the element
  Label holecheck_passed1;
  // @TODO(joransiu) : Fold AddP into memref of LoadP
  __ AddP(address, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ SmiToPtrArrayOffset(scratch, key);
  __ LoadP(scratch, MemOperand(address, scratch));
  __ CmpP(scratch, Operand(masm->isolate()->factory()->the_hole_value()));
  __ bne(&holecheck_passed1, Label::kNear);
  __ JumpIfDictionaryInPrototypeChain(receiver, elements_map, scratch, slow);

  __ bind(&holecheck_passed1);

  // Smi stores don't require further checks.
  Label non_smi_value;
  __ JumpIfNotSmi(value, &non_smi_value);

  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ AddSmiLiteral(scratch, key, Smi::FromInt(1), r0);
    __ StoreP(scratch, FieldMemOperand(receiver, JSArray::kLengthOffset));
  }
  // It's irrelevant whether array is smi-only or not when writing a smi.
  __ AddP(address, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ SmiToPtrArrayOffset(scratch, key);
  __ StoreP(value, MemOperand(address, scratch));
  __ Ret();

  __ bind(&non_smi_value);
  // Escape to elements kind transition case.
  __ CheckFastObjectElements(receiver_map, scratch, &transition_smi_elements);

  // Fast elements array, store the value to the elements backing store.
  __ bind(&finish_object_store);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ AddSmiLiteral(scratch, key, Smi::FromInt(1), r0);
    __ StoreP(scratch, FieldMemOperand(receiver, JSArray::kLengthOffset));
  }
  __ AddP(address, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ SmiToPtrArrayOffset(scratch, key);
  __ StoreP(value, MemOperand(address, scratch));
  __ la(address, MemOperand(address, scratch));
  // Update write barrier for the elements array address.
  __ LoadRR(scratch, value);  // Preserve the value which is returned.
  __ RecordWrite(elements, address, scratch, kLRHasNotBeenSaved,
                 kDontSaveFPRegs, EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  __ Ret();

  __ bind(fast_double);
  if (check_map == kCheckMap) {
    // Check for fast double array case. If this fails, call through to the
    // runtime.
    __ CompareRoot(elements_map, Heap::kFixedDoubleArrayMapRootIndex);
    __ bne(slow);
  }

  // HOLECHECK: guards "A[i] double hole?"
  // We have to see if the double version of the hole is present. If so
  // go to the runtime.
  // @TODO(joransiu) : Fold AddP Operand into LoadlW
  __ AddP(address, elements,
          Operand((FixedDoubleArray::kHeaderSize + Register::kExponentOffset -
                   kHeapObjectTag)));
  __ SmiToDoubleArrayOffset(scratch, key);
  __ LoadlW(scratch, MemOperand(address, scratch));
  __ CmpP(scratch, Operand(kHoleNanUpper32));
  __ bne(&fast_double_without_map_check, Label::kNear);
  __ JumpIfDictionaryInPrototypeChain(receiver, elements_map, scratch, slow);

  __ bind(&fast_double_without_map_check);
  __ StoreNumberToDoubleElements(value, key, elements, scratch, d0,
                                 &transition_double_elements);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ AddSmiLiteral(scratch, key, Smi::FromInt(1), r0);
    __ StoreP(scratch, FieldMemOperand(receiver, JSArray::kLengthOffset));
  }
  __ Ret();

  __ bind(&transition_smi_elements);
  // Transition the array appropriately depending on the value type.
  __ LoadP(scratch, FieldMemOperand(value, HeapObject::kMapOffset));
  __ CompareRoot(scratch, Heap::kHeapNumberMapRootIndex);
  __ bne(&non_double_value);

  // Value is a double. Transition FAST_SMI_ELEMENTS ->
  // FAST_DOUBLE_ELEMENTS and complete the store.
  __ LoadTransitionedArrayMapConditional(
      FAST_SMI_ELEMENTS, FAST_DOUBLE_ELEMENTS, receiver_map, scratch, slow);
  AllocationSiteMode mode =
      AllocationSite::GetMode(FAST_SMI_ELEMENTS, FAST_DOUBLE_ELEMENTS);
  ElementsTransitionGenerator::GenerateSmiToDouble(masm, receiver, key, value,
                                                   receiver_map, mode, slow);
  __ LoadP(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ b(&fast_double_without_map_check);

  __ bind(&non_double_value);
  // Value is not a double, FAST_SMI_ELEMENTS -> FAST_ELEMENTS
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS, FAST_ELEMENTS,
                                         receiver_map, scratch, slow);
  mode = AllocationSite::GetMode(FAST_SMI_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
      masm, receiver, key, value, receiver_map, mode, slow);
  __ LoadP(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ b(&finish_object_store);

  __ bind(&transition_double_elements);
  // Elements are FAST_DOUBLE_ELEMENTS, but value is an Object that's not a
  // HeapNumber. Make sure that the receiver is a Array with FAST_ELEMENTS and
  // transition array from FAST_DOUBLE_ELEMENTS to FAST_ELEMENTS
  __ LoadTransitionedArrayMapConditional(FAST_DOUBLE_ELEMENTS, FAST_ELEMENTS,
                                         receiver_map, scratch, slow);
  mode = AllocationSite::GetMode(FAST_DOUBLE_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateDoubleToObject(
      masm, receiver, key, value, receiver_map, mode, slow);
  __ LoadP(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ b(&finish_object_store);
}

void KeyedStoreIC::GenerateMegamorphic(MacroAssembler* masm,
                                       LanguageMode language_mode) {
  // ---------- S t a t e --------------
  //  -- r2     : value
  //  -- r3     : key
  //  -- r4     : receiver
  //  -- lr     : return address
  // -----------------------------------
  Label slow, fast_object, fast_object_grow;
  Label fast_double, fast_double_grow;
  Label array, extra, check_if_double_array, maybe_name_key, miss;

  // Register usage.
  Register value = StoreDescriptor::ValueRegister();
  Register key = StoreDescriptor::NameRegister();
  Register receiver = StoreDescriptor::ReceiverRegister();
  DCHECK(receiver.is(r3));
  DCHECK(key.is(r4));
  DCHECK(value.is(r2));
  Register receiver_map = r5;
  Register elements_map = r8;
  Register elements = r9;  // Elements array of the receiver.
  // r6 and r7 are used as general scratch registers.

  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &maybe_name_key);
  // Check that the object isn't a smi.
  __ JumpIfSmi(receiver, &slow);
  // Get the map of the object.
  __ LoadP(receiver_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.
  // The generic stub does not perform map checks.
  __ LoadlB(ip, FieldMemOperand(receiver_map, Map::kBitFieldOffset));
  __ AndP(r0, ip, Operand(1 << Map::kIsAccessCheckNeeded));
  __ bne(&slow, Label::kNear);
  // Check if the object is a JS array or not.
  __ LoadlB(r6, FieldMemOperand(receiver_map, Map::kInstanceTypeOffset));
  __ CmpP(r6, Operand(JS_ARRAY_TYPE));
  __ beq(&array);
  // Check that the object is some kind of JSObject.
  __ CmpP(r6, Operand(FIRST_JS_OBJECT_TYPE));
  __ blt(&slow, Label::kNear);

  // Object case: Check key against length in the elements array.
  __ LoadP(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  // Check array bounds. Both the key and the length of FixedArray are smis.
  __ CmpLogicalP(key, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ blt(&fast_object);

  // Slow case, handle jump to runtime.
  __ bind(&slow);
  // Entry registers are intact.
  // r2: value.
  // r3: key.
  // r4: receiver.
  PropertyICCompiler::GenerateRuntimeSetProperty(masm, language_mode);
  // Never returns to here.

  __ bind(&maybe_name_key);
  __ LoadP(r6, FieldMemOperand(key, HeapObject::kMapOffset));
  __ LoadlB(r6, FieldMemOperand(r6, Map::kInstanceTypeOffset));
  __ JumpIfNotUniqueNameInstanceType(r6, &slow);

  // The handlers in the stub cache expect a vector and slot. Since we won't
  // change the IC from any downstream misses, a dummy vector can be used.
  Register vector = StoreWithVectorDescriptor::VectorRegister();
  Register slot = StoreWithVectorDescriptor::SlotRegister();
  DCHECK(!AreAliased(vector, slot, r7, r8, r9, ip));
  Handle<TypeFeedbackVector> dummy_vector =
      TypeFeedbackVector::DummyVector(masm->isolate());
  int slot_index = dummy_vector->GetIndex(
      FeedbackVectorSlot(TypeFeedbackVector::kDummyKeyedStoreICSlot));
  __ LoadRoot(vector, Heap::kDummyVectorRootIndex);
  __ LoadSmiLiteral(slot, Smi::FromInt(slot_index));

  masm->isolate()->store_stub_cache()->GenerateProbe(masm, receiver, key, r7,
                                                     r8, r9, ip);
  // Cache miss.
  __ b(&miss);

  // Extra capacity case: Check if there is extra capacity to
  // perform the store and update the length. Used for adding one
  // element to the array by writing to array[array.length].
  __ bind(&extra);
  // Condition code from comparing key and array length is still available.
  __ bne(&slow);  // Only support writing to writing to array[array.length].
  // Check for room in the elements backing store.
  // Both the key and the length of FixedArray are smis.
  __ CmpLogicalP(key, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ bge(&slow);
  __ LoadP(elements_map, FieldMemOperand(elements, HeapObject::kMapOffset));
  __ CmpP(elements_map, Operand(masm->isolate()->factory()->fixed_array_map()));
  __ bne(&check_if_double_array, Label::kNear);
  __ b(&fast_object_grow);

  __ bind(&check_if_double_array);
  __ CmpP(elements_map,
          Operand(masm->isolate()->factory()->fixed_double_array_map()));
  __ bne(&slow);
  __ b(&fast_double_grow);

  // Array case: Get the length and the elements array from the JS
  // array. Check that the array is in fast mode (and writable); if it
  // is the length is always a smi.
  __ bind(&array);
  __ LoadP(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));

  // Check the key against the length in the array.
  __ CmpLogicalP(key, FieldMemOperand(receiver, JSArray::kLengthOffset));
  __ bge(&extra);

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

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kStoreIC_Miss);
}

void StoreIC::GenerateNormal(MacroAssembler* masm) {
  Label miss;
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  Register value = StoreDescriptor::ValueRegister();
  Register dictionary = r7;
  DCHECK(receiver.is(r3));
  DCHECK(name.is(r4));
  DCHECK(value.is(r2));
  DCHECK(StoreWithVectorDescriptor::VectorRegister().is(r5));
  DCHECK(StoreWithVectorDescriptor::SlotRegister().is(r6));

  __ LoadP(dictionary, FieldMemOperand(receiver, JSObject::kPropertiesOffset));

  GenerateDictionaryStore(masm, &miss, dictionary, name, value, r8, r9);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->ic_store_normal_hit(), 1, r8, r9);
  __ Ret();

  __ bind(&miss);
  __ IncrementCounter(counters->ic_store_normal_miss(), 1, r8, r9);
  GenerateMiss(masm);
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
      return gt;
    case Token::LTE:
      return le;
    case Token::GTE:
      return ge;
    default:
      UNREACHABLE();
      return kNoCondition;
  }
}

bool CompareIC::HasInlinedSmiCode(Address address) {
  // The address of the instruction following the call.
  Address cmp_instruction_address =
      Assembler::return_address_from_call_start(address);

  // If the instruction following the call is not a CHI, nothing
  // was inlined.
  return (Instruction::S390OpcodeValue(cmp_instruction_address) == CHI);
}

//
// This code is paired with the JumpPatchSite class in full-codegen-s390.cc
//
void PatchInlinedSmiCode(Isolate* isolate, Address address,
                         InlinedSmiCheck check) {
  Address cmp_instruction_address =
      Assembler::return_address_from_call_start(address);

  // If the instruction following the call is not a cmp rx, #yyy, nothing
  // was inlined.
  Instr instr = Assembler::instr_at(cmp_instruction_address);
  if (Instruction::S390OpcodeValue(cmp_instruction_address) != CHI) {
    return;
  }

  if (Instruction::S390OpcodeValue(address) != BRASL) {
    return;
  }
  // The delta to the start of the map check instruction and the
  // condition code uses at the patched jump.
  int delta = instr & 0x0000ffff;

  // If the delta is 0 the instruction is cmp r0, #0 which also signals that
  // nothing was inlined.
  if (delta == 0) {
    return;
  }

  if (FLAG_trace_ic) {
    PrintF("[  patching ic at %p, cmp=%p, delta=%d\n",
           static_cast<void*>(address),
           static_cast<void*>(cmp_instruction_address), delta);
  }

  // Expected sequence to enable by changing the following
  //   CR/CGR  Rx, Rx    // 2 / 4 bytes
  //   LR  R0, R0        // 2 bytes   // 31-bit only!
  //   BRC/BRCL          // 4 / 6 bytes
  // into
  //   TMLL    Rx, XXX   // 4 bytes
  //   BRC/BRCL          // 4 / 6 bytes
  // And vice versa to disable.

  // The following constant is the size of the CR/CGR + LR + LR
  const int kPatchAreaSizeNoBranch = 4;
  Address patch_address = cmp_instruction_address - delta;
  Address branch_address = patch_address + kPatchAreaSizeNoBranch;

  Instr instr_at_patch = Assembler::instr_at(patch_address);
  SixByteInstr branch_instr = Assembler::instr_at(branch_address);

  // This is patching a conditional "jump if not smi/jump if smi" site.
  size_t patch_size = 0;
  if (Instruction::S390OpcodeValue(branch_address) == BRC) {
    patch_size = kPatchAreaSizeNoBranch + 4;
  } else if (Instruction::S390OpcodeValue(branch_address) == BRCL) {
    patch_size = kPatchAreaSizeNoBranch + 6;
  } else {
    DCHECK(false);
  }
  CodePatcher patcher(isolate, patch_address, patch_size);
  Register reg;
  reg.reg_code = instr_at_patch & 0xf;
  if (check == ENABLE_INLINED_SMI_CHECK) {
    patcher.masm()->TestIfSmi(reg);
  } else {
    // Emit the NOP to ensure sufficient place for patching
    // (replaced by LR + NILL)
    DCHECK(check == DISABLE_INLINED_SMI_CHECK);
    patcher.masm()->CmpP(reg, reg);
#ifndef V8_TARGET_ARCH_S390X
    patcher.masm()->nop();
#endif
  }

  Condition cc = al;
  if (Instruction::S390OpcodeValue(branch_address) == BRC) {
    cc = static_cast<Condition>((branch_instr & 0x00f00000) >> 20);
    DCHECK((cc == ne) || (cc == eq));
    cc = (cc == ne) ? eq : ne;
    patcher.masm()->brc(cc, Operand((branch_instr & 0xffff) << 1));
  } else if (Instruction::S390OpcodeValue(branch_address) == BRCL) {
    cc = static_cast<Condition>(
        (branch_instr & (static_cast<uint64_t>(0x00f0) << 32)) >> 36);
    DCHECK((cc == ne) || (cc == eq));
    cc = (cc == ne) ? eq : ne;
    patcher.masm()->brcl(cc, Operand((branch_instr & 0xffffffff) << 1));
  } else {
    DCHECK(false);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
