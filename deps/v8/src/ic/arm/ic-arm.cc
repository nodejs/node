// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

#include "src/codegen.h"
#include "src/ic/ic.h"
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
  __ cmp(type, Operand(JS_GLOBAL_OBJECT_TYPE));
  __ b(eq, global_object);
  __ cmp(type, Operand(JS_GLOBAL_PROXY_TYPE));
  __ b(eq, global_object);
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
  const int kTypeAndReadOnlyMask =
      (PropertyDetails::TypeField::kMask |
       PropertyDetails::AttributesField::encode(READ_ONLY))
      << kSmiTagSize;
  __ ldr(scratch1, FieldMemOperand(scratch2, kDetailsOffset));
  __ tst(scratch1, Operand(kTypeAndReadOnlyMask));
  __ b(ne, miss);

  // Store the value at the masked, scaled index and return.
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ add(scratch2, scratch2, Operand(kValueOffset - kHeapObjectTag));
  __ str(value, MemOperand(scratch2));

  // Update the write barrier. Make sure not to clobber the value.
  __ mov(scratch1, value);
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
  DCHECK(JS_OBJECT_TYPE > JS_VALUE_TYPE);
  __ ldrb(scratch, FieldMemOperand(map, Map::kInstanceTypeOffset));
  __ cmp(scratch, Operand(JS_OBJECT_TYPE));
  __ b(lt, slow);
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
  // elements - holds the elements of the receiver and its prototypes.
  //
  // scratch1 - used to hold elements length, bit fields, base addresses.
  //
  // scratch2 - used to hold maps, prototypes, and the loaded value.
  Label check_prototypes, check_next_prototype;
  Label done, in_bounds, absent;

  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ AssertFastElements(elements);

  // Check that the key (index) is within bounds.
  __ ldr(scratch1, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ cmp(key, Operand(scratch1));
  __ b(lo, &in_bounds);
  // Out-of-bounds. Check the prototype chain to see if we can just return
  // 'undefined'.
  __ cmp(key, Operand(0));
  __ b(lt, slow);  // Negative keys can't take the fast OOB path.
  __ bind(&check_prototypes);
  __ ldr(scratch2, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ bind(&check_next_prototype);
  __ ldr(scratch2, FieldMemOperand(scratch2, Map::kPrototypeOffset));
  // scratch2: current prototype
  __ CompareRoot(scratch2, Heap::kNullValueRootIndex);
  __ b(eq, &absent);
  __ ldr(elements, FieldMemOperand(scratch2, JSObject::kElementsOffset));
  __ ldr(scratch2, FieldMemOperand(scratch2, HeapObject::kMapOffset));
  // elements: elements of current prototype
  // scratch2: map of current prototype
  __ CompareInstanceType(scratch2, scratch1, JS_OBJECT_TYPE);
  __ b(lo, slow);
  __ ldrb(scratch1, FieldMemOperand(scratch2, Map::kBitFieldOffset));
  __ tst(scratch1, Operand((1 << Map::kIsAccessCheckNeeded) |
                           (1 << Map::kHasIndexedInterceptor)));
  __ b(ne, slow);
  __ CompareRoot(elements, Heap::kEmptyFixedArrayRootIndex);
  __ b(ne, slow);
  __ jmp(&check_next_prototype);

  __ bind(&absent);
  __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
  __ jmp(&done);

  __ bind(&in_bounds);
  // Fast case: Do the load.
  __ add(scratch1, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ ldr(scratch2, MemOperand::PointerAddressFromSmiKey(scratch1, key));
  __ CompareRoot(scratch2, Heap::kTheHoleValueRootIndex);
  // In case the loaded value is the_hole we have to check the prototype chain.
  __ b(eq, &check_prototypes);
  __ mov(result, scratch2);
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
  __ b(hi, not_unique);
  STATIC_ASSERT(LAST_UNIQUE_NAME_TYPE == FIRST_NONSTRING_TYPE);
  __ b(eq, &unique);

  // Is the string an array index, with cached numeric value?
  __ ldr(hash, FieldMemOperand(key, Name::kHashFieldOffset));
  __ tst(hash, Operand(Name::kContainsCachedArrayIndexMask));
  __ b(eq, index_string);

  // Is the string internalized? We know it's a string, so a single
  // bit test is enough.
  // map: key map
  __ ldrb(hash, FieldMemOperand(map, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0);
  __ tst(hash, Operand(kIsNotInternalizedMask));
  __ b(ne, not_unique);

  __ bind(&unique);
}

void LoadIC::GenerateNormal(MacroAssembler* masm) {
  Register dictionary = r0;
  DCHECK(!dictionary.is(LoadDescriptor::ReceiverRegister()));
  DCHECK(!dictionary.is(LoadDescriptor::NameRegister()));

  Label slow;

  __ ldr(dictionary, FieldMemOperand(LoadDescriptor::ReceiverRegister(),
                                     JSObject::kPropertiesOffset));
  GenerateDictionaryLoad(masm, &slow, dictionary,
                         LoadDescriptor::NameRegister(), r0, r3, r4);
  __ Ret();

  // Dictionary load failed, go slow (but don't miss).
  __ bind(&slow);
  GenerateRuntimeGetProperty(masm);
}


// A register that isn't one of the parameters to the load ic.
static const Register LoadIC_TempRegister() { return r3; }


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

  DCHECK(!AreAliased(r4, r5, LoadWithVectorDescriptor::SlotRegister(),
                     LoadWithVectorDescriptor::VectorRegister()));
  __ IncrementCounter(isolate->counters()->ic_load_miss(), 1, r4, r5);

  LoadIC_PushArgs(masm);

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kLoadIC_Miss);
}

void LoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // The return address is in lr.

  __ mov(LoadIC_TempRegister(), LoadDescriptor::ReceiverRegister());
  __ Push(LoadIC_TempRegister(), LoadDescriptor::NameRegister());

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kGetProperty);
}


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  // The return address is in lr.
  Isolate* isolate = masm->isolate();

  DCHECK(!AreAliased(r4, r5, LoadWithVectorDescriptor::SlotRegister(),
                     LoadWithVectorDescriptor::VectorRegister()));
  __ IncrementCounter(isolate->counters()->ic_keyed_load_miss(), 1, r4, r5);

  LoadIC_PushArgs(masm);

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kKeyedLoadIC_Miss);
}

void KeyedLoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // The return address is in lr.

  __ Push(LoadDescriptor::ReceiverRegister(), LoadDescriptor::NameRegister());

  // Perform tail call to the entry.
  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kKeyedGetProperty);
}

void KeyedLoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  // The return address is in lr.
  Label slow, check_name, index_smi, index_name, property_array_property;
  Label probe_dictionary, check_number_dictionary;

  Register key = LoadDescriptor::NameRegister();
  Register receiver = LoadDescriptor::ReceiverRegister();
  DCHECK(key.is(r2));
  DCHECK(receiver.is(r1));

  Isolate* isolate = masm->isolate();

  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &check_name);
  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from below
  // where a numeric string is converted to a smi.

  GenerateKeyedLoadReceiverCheck(masm, receiver, r0, r3,
                                 Map::kHasIndexedInterceptor, &slow);

  // Check the receiver's map to see if it has fast elements.
  __ CheckFastElements(r0, r3, &check_number_dictionary);

  GenerateFastArrayLoad(masm, receiver, key, r0, r3, r4, r0, &slow);
  __ IncrementCounter(isolate->counters()->ic_keyed_load_generic_smi(), 1, r4,
                      r3);
  __ Ret();

  __ bind(&check_number_dictionary);
  __ ldr(r4, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ ldr(r3, FieldMemOperand(r4, JSObject::kMapOffset));

  // Check whether the elements is a number dictionary.
  // r3: elements map
  // r4: elements
  __ LoadRoot(ip, Heap::kHashTableMapRootIndex);
  __ cmp(r3, ip);
  __ b(ne, &slow);
  __ SmiUntag(r0, key);
  __ LoadFromNumberDictionary(&slow, r4, key, r0, r0, r3, r5);
  __ Ret();

  // Slow case, key and receiver still in r2 and r1.
  __ bind(&slow);
  __ IncrementCounter(isolate->counters()->ic_keyed_load_generic_slow(), 1, r4,
                      r3);
  GenerateRuntimeGetProperty(masm);

  __ bind(&check_name);
  GenerateKeyNameCheck(masm, key, r0, r3, &index_name, &slow);

  GenerateKeyedLoadReceiverCheck(masm, receiver, r0, r3,
                                 Map::kHasNamedInterceptor, &slow);

  // If the receiver is a fast-case object, check the stub cache. Otherwise
  // probe the dictionary.
  __ ldr(r3, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ ldr(r4, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHashTableMapRootIndex);
  __ cmp(r4, ip);
  __ b(eq, &probe_dictionary);

  // The handlers in the stub cache expect a vector and slot. Since we won't
  // change the IC from any downstream misses, a dummy vector can be used.
  Register vector = LoadWithVectorDescriptor::VectorRegister();
  Register slot = LoadWithVectorDescriptor::SlotRegister();
  DCHECK(!AreAliased(vector, slot, r4, r5, r6, r9));
  Handle<TypeFeedbackVector> dummy_vector =
      TypeFeedbackVector::DummyVector(masm->isolate());
  int slot_index = dummy_vector->GetIndex(
      FeedbackVectorSlot(TypeFeedbackVector::kDummyKeyedLoadICSlot));
  __ LoadRoot(vector, Heap::kDummyVectorRootIndex);
  __ mov(slot, Operand(Smi::FromInt(slot_index)));

  masm->isolate()->load_stub_cache()->GenerateProbe(masm, receiver, key, r4, r5,
                                                    r6, r9);
  // Cache miss.
  GenerateMiss(masm);

  // Do a quick inline probe of the receiver's dictionary, if it
  // exists.
  __ bind(&probe_dictionary);
  // r3: elements
  __ ldr(r0, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ ldrb(r0, FieldMemOperand(r0, Map::kInstanceTypeOffset));
  GenerateGlobalInstanceTypeCheck(masm, r0, &slow);
  // Load the property to r0.
  GenerateDictionaryLoad(masm, &slow, r3, key, r0, r5, r4);
  __ IncrementCounter(isolate->counters()->ic_keyed_load_generic_symbol(), 1,
                      r4, r3);
  __ Ret();

  __ bind(&index_name);
  __ IndexFromHash(r3, key);
  // Now jump to the place where smi keys are handled.
  __ jmp(&index_smi);
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
  Register scratch = r4;
  Register address = r5;
  DCHECK(!AreAliased(value, key, receiver, receiver_map, elements_map, elements,
                     scratch, address));

  if (check_map == kCheckMap) {
    __ ldr(elements_map, FieldMemOperand(elements, HeapObject::kMapOffset));
    __ cmp(elements_map,
           Operand(masm->isolate()->factory()->fixed_array_map()));
    __ b(ne, fast_double);
  }

  // HOLECHECK: guards "A[i] = V"
  // We have to go to the runtime if the current value is the hole because
  // there may be a callback on the element
  Label holecheck_passed1;
  __ add(address, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ ldr(scratch, MemOperand::PointerAddressFromSmiKey(address, key, PreIndex));
  __ cmp(scratch, Operand(masm->isolate()->factory()->the_hole_value()));
  __ b(ne, &holecheck_passed1);
  __ JumpIfDictionaryInPrototypeChain(receiver, elements_map, scratch, slow);

  __ bind(&holecheck_passed1);

  // Smi stores don't require further checks.
  Label non_smi_value;
  __ JumpIfNotSmi(value, &non_smi_value);

  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ add(scratch, key, Operand(Smi::FromInt(1)));
    __ str(scratch, FieldMemOperand(receiver, JSArray::kLengthOffset));
  }
  // It's irrelevant whether array is smi-only or not when writing a smi.
  __ add(address, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ str(value, MemOperand::PointerAddressFromSmiKey(address, key));
  __ Ret();

  __ bind(&non_smi_value);
  // Escape to elements kind transition case.
  __ CheckFastObjectElements(receiver_map, scratch, &transition_smi_elements);

  // Fast elements array, store the value to the elements backing store.
  __ bind(&finish_object_store);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ add(scratch, key, Operand(Smi::FromInt(1)));
    __ str(scratch, FieldMemOperand(receiver, JSArray::kLengthOffset));
  }
  __ add(address, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ add(address, address, Operand::PointerOffsetFromSmiKey(key));
  __ str(value, MemOperand(address));
  // Update write barrier for the elements array address.
  __ mov(scratch, value);  // Preserve the value which is returned.
  __ RecordWrite(elements, address, scratch, kLRHasNotBeenSaved,
                 kDontSaveFPRegs, EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  __ Ret();

  __ bind(fast_double);
  if (check_map == kCheckMap) {
    // Check for fast double array case. If this fails, call through to the
    // runtime.
    __ CompareRoot(elements_map, Heap::kFixedDoubleArrayMapRootIndex);
    __ b(ne, slow);
  }

  // HOLECHECK: guards "A[i] double hole?"
  // We have to see if the double version of the hole is present. If so
  // go to the runtime.
  __ add(address, elements,
         Operand((FixedDoubleArray::kHeaderSize + sizeof(kHoleNanLower32)) -
                 kHeapObjectTag));
  __ ldr(scratch, MemOperand(address, key, LSL, kPointerSizeLog2, PreIndex));
  __ cmp(scratch, Operand(kHoleNanUpper32));
  __ b(ne, &fast_double_without_map_check);
  __ JumpIfDictionaryInPrototypeChain(receiver, elements_map, scratch, slow);

  __ bind(&fast_double_without_map_check);
  __ StoreNumberToDoubleElements(value, key, elements, scratch, d0,
                                 &transition_double_elements);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ add(scratch, key, Operand(Smi::FromInt(1)));
    __ str(scratch, FieldMemOperand(receiver, JSArray::kLengthOffset));
  }
  __ Ret();

  __ bind(&transition_smi_elements);
  // Transition the array appropriately depending on the value type.
  __ ldr(scratch, FieldMemOperand(value, HeapObject::kMapOffset));
  __ CompareRoot(scratch, Heap::kHeapNumberMapRootIndex);
  __ b(ne, &non_double_value);

  // Value is a double. Transition FAST_SMI_ELEMENTS ->
  // FAST_DOUBLE_ELEMENTS and complete the store.
  __ LoadTransitionedArrayMapConditional(
      FAST_SMI_ELEMENTS, FAST_DOUBLE_ELEMENTS, receiver_map, scratch, slow);
  AllocationSiteMode mode =
      AllocationSite::GetMode(FAST_SMI_ELEMENTS, FAST_DOUBLE_ELEMENTS);
  ElementsTransitionGenerator::GenerateSmiToDouble(masm, receiver, key, value,
                                                   receiver_map, mode, slow);
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ jmp(&fast_double_without_map_check);

  __ bind(&non_double_value);
  // Value is not a double, FAST_SMI_ELEMENTS -> FAST_ELEMENTS
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS, FAST_ELEMENTS,
                                         receiver_map, scratch, slow);
  mode = AllocationSite::GetMode(FAST_SMI_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
      masm, receiver, key, value, receiver_map, mode, slow);
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ jmp(&finish_object_store);

  __ bind(&transition_double_elements);
  // Elements are FAST_DOUBLE_ELEMENTS, but value is an Object that's not a
  // HeapNumber. Make sure that the receiver is a Array with FAST_ELEMENTS and
  // transition array from FAST_DOUBLE_ELEMENTS to FAST_ELEMENTS
  __ LoadTransitionedArrayMapConditional(FAST_DOUBLE_ELEMENTS, FAST_ELEMENTS,
                                         receiver_map, scratch, slow);
  mode = AllocationSite::GetMode(FAST_DOUBLE_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateDoubleToObject(
      masm, receiver, key, value, receiver_map, mode, slow);
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ jmp(&finish_object_store);
}


void KeyedStoreIC::GenerateMegamorphic(MacroAssembler* masm,
                                       LanguageMode language_mode) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- r1     : key
  //  -- r2     : receiver
  //  -- lr     : return address
  // -----------------------------------
  Label slow, fast_object, fast_object_grow;
  Label fast_double, fast_double_grow;
  Label array, extra, check_if_double_array, maybe_name_key, miss;

  // Register usage.
  Register value = StoreDescriptor::ValueRegister();
  Register key = StoreDescriptor::NameRegister();
  Register receiver = StoreDescriptor::ReceiverRegister();
  DCHECK(receiver.is(r1));
  DCHECK(key.is(r2));
  DCHECK(value.is(r0));
  Register receiver_map = r3;
  Register elements_map = r6;
  Register elements = r9;  // Elements array of the receiver.
  // r4 and r5 are used as general scratch registers.

  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &maybe_name_key);
  // Check that the object isn't a smi.
  __ JumpIfSmi(receiver, &slow);
  // Get the map of the object.
  __ ldr(receiver_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks.
  // The generic stub does not perform map checks.
  __ ldrb(ip, FieldMemOperand(receiver_map, Map::kBitFieldOffset));
  __ tst(ip, Operand(1 << Map::kIsAccessCheckNeeded));
  __ b(ne, &slow);
  // Check if the object is a JS array or not.
  __ ldrb(r4, FieldMemOperand(receiver_map, Map::kInstanceTypeOffset));
  __ cmp(r4, Operand(JS_ARRAY_TYPE));
  __ b(eq, &array);
  // Check that the object is some kind of JS object EXCEPT JS Value type. In
  // the case that the object is a value-wrapper object, we enter the runtime
  // system to make sure that indexing into string objects works as intended.
  STATIC_ASSERT(JS_VALUE_TYPE < JS_OBJECT_TYPE);
  __ cmp(r4, Operand(JS_OBJECT_TYPE));
  __ b(lo, &slow);

  // Object case: Check key against length in the elements array.
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  // Check array bounds. Both the key and the length of FixedArray are smis.
  __ ldr(ip, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ cmp(key, Operand(ip));
  __ b(lo, &fast_object);

  // Slow case, handle jump to runtime.
  __ bind(&slow);
  // Entry registers are intact.
  // r0: value.
  // r1: key.
  // r2: receiver.
  PropertyICCompiler::GenerateRuntimeSetProperty(masm, language_mode);
  // Never returns to here.

  __ bind(&maybe_name_key);
  __ ldr(r4, FieldMemOperand(key, HeapObject::kMapOffset));
  __ ldrb(r4, FieldMemOperand(r4, Map::kInstanceTypeOffset));
  __ JumpIfNotUniqueNameInstanceType(r4, &slow);

  // We use register r8, because otherwise probing the megamorphic stub cache
  // would require pushing temporaries on the stack.
  // TODO(mvstanton): quit using register r8 when
  // FLAG_enable_embedded_constant_pool is turned on.
  DCHECK(!FLAG_enable_embedded_constant_pool);
  Register temporary2 = r8;
  // The handlers in the stub cache expect a vector and slot. Since we won't
  // change the IC from any downstream misses, a dummy vector can be used.
  Register vector = StoreWithVectorDescriptor::VectorRegister();
  Register slot = StoreWithVectorDescriptor::SlotRegister();

  DCHECK(!AreAliased(vector, slot, r5, temporary2, r6, r9));
  Handle<TypeFeedbackVector> dummy_vector =
      TypeFeedbackVector::DummyVector(masm->isolate());
  int slot_index = dummy_vector->GetIndex(
      FeedbackVectorSlot(TypeFeedbackVector::kDummyKeyedStoreICSlot));
  __ LoadRoot(vector, Heap::kDummyVectorRootIndex);
  __ mov(slot, Operand(Smi::FromInt(slot_index)));

  masm->isolate()->store_stub_cache()->GenerateProbe(masm, receiver, key, r5,
                                                     temporary2, r6, r9);
  // Cache miss.
  __ b(&miss);

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
  __ ldr(elements_map, FieldMemOperand(elements, HeapObject::kMapOffset));
  __ cmp(elements_map, Operand(masm->isolate()->factory()->fixed_array_map()));
  __ b(ne, &check_if_double_array);
  __ jmp(&fast_object_grow);

  __ bind(&check_if_double_array);
  __ cmp(elements_map,
         Operand(masm->isolate()->factory()->fixed_double_array_map()));
  __ b(ne, &slow);
  __ jmp(&fast_double_grow);

  // Array case: Get the length and the elements array from the JS
  // array. Check that the array is in fast mode (and writable); if it
  // is the length is always a smi.
  __ bind(&array);
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));

  // Check the key against the length in the array.
  __ ldr(ip, FieldMemOperand(receiver, JSArray::kLengthOffset));
  __ cmp(key, Operand(ip));
  __ b(hs, &extra);

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
  Register dictionary = r5;
  DCHECK(receiver.is(r1));
  DCHECK(name.is(r2));
  DCHECK(value.is(r0));
  DCHECK(StoreWithVectorDescriptor::VectorRegister().is(r3));
  DCHECK(StoreWithVectorDescriptor::SlotRegister().is(r4));

  __ ldr(dictionary, FieldMemOperand(receiver, JSObject::kPropertiesOffset));

  GenerateDictionaryStore(masm, &miss, dictionary, name, value, r6, r9);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->ic_store_normal_hit(), 1, r6, r9);
  __ Ret();

  __ bind(&miss);
  __ IncrementCounter(counters->ic_store_normal_miss(), 1, r6, r9);
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

  // If the instruction following the call is not a cmp rx, #yyy, nothing
  // was inlined.
  Instr instr = Assembler::instr_at(cmp_instruction_address);
  return Assembler::IsCmpImmediate(instr);
}


void PatchInlinedSmiCode(Isolate* isolate, Address address,
                         InlinedSmiCheck check) {
  Address cmp_instruction_address =
      Assembler::return_address_from_call_start(address);

  // If the instruction following the call is not a cmp rx, #yyy, nothing
  // was inlined.
  Instr instr = Assembler::instr_at(cmp_instruction_address);
  if (!Assembler::IsCmpImmediate(instr)) {
    return;
  }

  // The delta to the start of the map check instruction and the
  // condition code uses at the patched jump.
  int delta = Assembler::GetCmpImmediateRawImmediate(instr);
  delta += Assembler::GetCmpImmediateRegister(instr).code() * kOff12Mask;
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

  Address patch_address =
      cmp_instruction_address - delta * Instruction::kInstrSize;
  Instr instr_at_patch = Assembler::instr_at(patch_address);
  Instr branch_instr =
      Assembler::instr_at(patch_address + Instruction::kInstrSize);
  // This is patching a conditional "jump if not smi/jump if smi" site.
  // Enabling by changing from
  //   cmp rx, rx
  //   b eq/ne, <target>
  // to
  //   tst rx, #kSmiTagMask
  //   b ne/eq, <target>
  // and vice-versa to be disabled again.
  CodePatcher patcher(isolate, patch_address, 2);
  Register reg = Assembler::GetRn(instr_at_patch);
  if (check == ENABLE_INLINED_SMI_CHECK) {
    DCHECK(Assembler::IsCmpRegister(instr_at_patch));
    DCHECK_EQ(Assembler::GetRn(instr_at_patch).code(),
              Assembler::GetRm(instr_at_patch).code());
    patcher.masm()->tst(reg, Operand(kSmiTagMask));
  } else {
    DCHECK(check == DISABLE_INLINED_SMI_CHECK);
    DCHECK(Assembler::IsTstImmediate(instr_at_patch));
    patcher.masm()->cmp(reg, reg);
  }
  DCHECK(Assembler::IsBranch(branch_instr));
  if (Assembler::GetCondition(branch_instr) == eq) {
    patcher.EmitCondition(ne);
  } else {
    DCHECK(Assembler::GetCondition(branch_instr) == ne);
    patcher.EmitCondition(eq);
  }
}
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM
