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


// "type" holds an instance type on entry and is not clobbered.
// Generated code branch on "global_object" if type is any kind of global
// JS object.
static void GenerateGlobalInstanceTypeCheck(MacroAssembler* masm, Register type,
                                            Label* global_object) {
  __ Cmp(type, JS_GLOBAL_OBJECT_TYPE);
  __ Ccmp(type, JS_GLOBAL_PROXY_TYPE, ZFlag, ne);
  __ B(eq, global_object);
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


// Checks the receiver for special cases (value type, slow case bits).
// Falls through for regular JS object and return the map of the
// receiver in 'map_scratch' if the receiver is not a SMI.
static void GenerateKeyedLoadReceiverCheck(MacroAssembler* masm,
                                           Register receiver,
                                           Register map_scratch,
                                           Register scratch,
                                           int interceptor_bit, Label* slow) {
  DCHECK(!AreAliased(map_scratch, scratch));

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
//
// receiver - holds the receiver on entry.
//            Unchanged unless 'result' is the same register.
//
// key      - holds the smi key on entry.
//            Unchanged unless 'result' is the same register.
//
// elements - holds the elements of the receiver and its prototypes. Clobbered.
//
// result   - holds the result on exit if the load succeeded.
//            Allowed to be the the same as 'receiver' or 'key'.
//            Unchanged on bailout so 'receiver' and 'key' can be safely
//            used by further computation.
static void GenerateFastArrayLoad(MacroAssembler* masm, Register receiver,
                                  Register key, Register elements,
                                  Register scratch1, Register scratch2,
                                  Register result, Label* slow,
                                  LanguageMode language_mode) {
  DCHECK(!AreAliased(receiver, key, elements, scratch1, scratch2));

  Label check_prototypes, check_next_prototype;
  Label done, in_bounds, absent;

  // Check for fast array.
  __ Ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ AssertFastElements(elements);

  // Check that the key (index) is within bounds.
  __ Ldr(scratch1, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ Cmp(key, scratch1);
  __ B(lo, &in_bounds);

  // Out of bounds. Check the prototype chain to see if we can just return
  // 'undefined'.
  __ Cmp(key, Operand(Smi::FromInt(0)));
  __ B(lt, slow);  // Negative keys can't take the fast OOB path.
  __ Bind(&check_prototypes);
  __ Ldr(scratch2, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Bind(&check_next_prototype);
  __ Ldr(scratch2, FieldMemOperand(scratch2, Map::kPrototypeOffset));
  // scratch2: current prototype
  __ JumpIfRoot(scratch2, Heap::kNullValueRootIndex, &absent);
  __ Ldr(elements, FieldMemOperand(scratch2, JSObject::kElementsOffset));
  __ Ldr(scratch2, FieldMemOperand(scratch2, HeapObject::kMapOffset));
  // elements: elements of current prototype
  // scratch2: map of current prototype
  __ CompareInstanceType(scratch2, scratch1, JS_OBJECT_TYPE);
  __ B(lo, slow);
  __ Ldrb(scratch1, FieldMemOperand(scratch2, Map::kBitFieldOffset));
  __ Tbnz(scratch1, Map::kIsAccessCheckNeeded, slow);
  __ Tbnz(scratch1, Map::kHasIndexedInterceptor, slow);
  __ JumpIfNotRoot(elements, Heap::kEmptyFixedArrayRootIndex, slow);
  __ B(&check_next_prototype);

  __ Bind(&absent);
  if (is_strong(language_mode)) {
    // Strong mode accesses must throw in this case, so call the runtime.
    __ B(slow);
  } else {
    __ LoadRoot(result, Heap::kUndefinedValueRootIndex);
    __ B(&done);
  }

  __ Bind(&in_bounds);
  // Fast case: Do the load.
  __ Add(scratch1, elements, FixedArray::kHeaderSize - kHeapObjectTag);
  __ SmiUntag(scratch2, key);
  __ Ldr(scratch2, MemOperand(scratch1, scratch2, LSL, kPointerSizeLog2));

  // In case the loaded value is the_hole we have to check the prototype chain.
  __ JumpIfRoot(scratch2, Heap::kTheHoleValueRootIndex, &check_prototypes);

  // Move the value to the result register.
  // 'result' can alias with 'receiver' or 'key' but these two must be
  // preserved if we jump to 'slow'.
  __ Mov(result, scratch2);
  __ Bind(&done);
}


// Checks whether a key is an array index string or a unique name.
// Falls through if a key is a unique name.
// The map of the key is returned in 'map_scratch'.
// If the jump to 'index_string' is done the hash of the key is left
// in 'hash_scratch'.
static void GenerateKeyNameCheck(MacroAssembler* masm, Register key,
                                 Register map_scratch, Register hash_scratch,
                                 Label* index_string, Label* not_unique) {
  DCHECK(!AreAliased(key, map_scratch, hash_scratch));

  // Is the key a name?
  Label unique;
  __ JumpIfObjectType(key, map_scratch, hash_scratch, LAST_UNIQUE_NAME_TYPE,
                      not_unique, hi);
  STATIC_ASSERT(LAST_UNIQUE_NAME_TYPE == FIRST_NONSTRING_TYPE);
  __ B(eq, &unique);

  // Is the string an array index with cached numeric value?
  __ Ldr(hash_scratch.W(), FieldMemOperand(key, Name::kHashFieldOffset));
  __ TestAndBranchIfAllClear(hash_scratch, Name::kContainsCachedArrayIndexMask,
                             index_string);

  // Is the string internalized? We know it's a string, so a single bit test is
  // enough.
  __ Ldrb(hash_scratch, FieldMemOperand(map_scratch, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0);
  __ TestAndBranchIfAnySet(hash_scratch, kIsNotInternalizedMask, not_unique);

  __ Bind(&unique);
  // Fall through if the key is a unique name.
}


void LoadIC::GenerateNormal(MacroAssembler* masm, LanguageMode language_mode) {
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
  GenerateRuntimeGetProperty(masm, language_mode);
}


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // The return address is in lr.
  Isolate* isolate = masm->isolate();
  ASM_LOCATION("LoadIC::GenerateMiss");

  DCHECK(!AreAliased(x4, x5, LoadWithVectorDescriptor::SlotRegister(),
                     LoadWithVectorDescriptor::VectorRegister()));
  __ IncrementCounter(isolate->counters()->load_miss(), 1, x4, x5);

  // Perform tail call to the entry.
  __ Push(LoadWithVectorDescriptor::ReceiverRegister(),
          LoadWithVectorDescriptor::NameRegister(),
          LoadWithVectorDescriptor::SlotRegister(),
          LoadWithVectorDescriptor::VectorRegister());
  __ TailCallRuntime(Runtime::kLoadIC_Miss);
}


void LoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm,
                                        LanguageMode language_mode) {
  // The return address is in lr.
  __ Push(LoadDescriptor::ReceiverRegister(), LoadDescriptor::NameRegister());

  // Do tail-call to runtime routine.
  __ TailCallRuntime(is_strong(language_mode) ? Runtime::kGetPropertyStrong
                                              : Runtime::kGetProperty);
}


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  // The return address is in lr.
  Isolate* isolate = masm->isolate();

  DCHECK(!AreAliased(x10, x11, LoadWithVectorDescriptor::SlotRegister(),
                     LoadWithVectorDescriptor::VectorRegister()));
  __ IncrementCounter(isolate->counters()->keyed_load_miss(), 1, x10, x11);

  __ Push(LoadWithVectorDescriptor::ReceiverRegister(),
          LoadWithVectorDescriptor::NameRegister(),
          LoadWithVectorDescriptor::SlotRegister(),
          LoadWithVectorDescriptor::VectorRegister());

  // Perform tail call to the entry.
  __ TailCallRuntime(Runtime::kKeyedLoadIC_Miss);
}


void KeyedLoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm,
                                             LanguageMode language_mode) {
  // The return address is in lr.
  __ Push(LoadDescriptor::ReceiverRegister(), LoadDescriptor::NameRegister());

  // Do tail-call to runtime routine.
  __ TailCallRuntime(is_strong(language_mode) ? Runtime::kKeyedGetPropertyStrong
                                              : Runtime::kKeyedGetProperty);
}


static void GenerateKeyedLoadWithSmiKey(MacroAssembler* masm, Register key,
                                        Register receiver, Register scratch1,
                                        Register scratch2, Register scratch3,
                                        Register scratch4, Register scratch5,
                                        Label* slow,
                                        LanguageMode language_mode) {
  DCHECK(!AreAliased(key, receiver, scratch1, scratch2, scratch3, scratch4,
                     scratch5));

  Isolate* isolate = masm->isolate();
  Label check_number_dictionary;
  // If we can load the value, it should be returned in x0.
  Register result = x0;

  GenerateKeyedLoadReceiverCheck(masm, receiver, scratch1, scratch2,
                                 Map::kHasIndexedInterceptor, slow);

  // Check the receiver's map to see if it has fast elements.
  __ CheckFastElements(scratch1, scratch2, &check_number_dictionary);

  GenerateFastArrayLoad(masm, receiver, key, scratch3, scratch2, scratch1,
                        result, slow, language_mode);
  __ IncrementCounter(isolate->counters()->keyed_load_generic_smi(), 1,
                      scratch1, scratch2);
  __ Ret();

  __ Bind(&check_number_dictionary);
  __ Ldr(scratch3, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ Ldr(scratch2, FieldMemOperand(scratch3, JSObject::kMapOffset));

  // Check whether we have a number dictionary.
  __ JumpIfNotRoot(scratch2, Heap::kHashTableMapRootIndex, slow);

  __ LoadFromNumberDictionary(slow, scratch3, key, result, scratch1, scratch2,
                              scratch4, scratch5);
  __ Ret();
}

static void GenerateKeyedLoadWithNameKey(MacroAssembler* masm, Register key,
                                         Register receiver, Register scratch1,
                                         Register scratch2, Register scratch3,
                                         Register scratch4, Register scratch5,
                                         Label* slow) {
  DCHECK(!AreAliased(key, receiver, scratch1, scratch2, scratch3, scratch4,
                     scratch5));

  Isolate* isolate = masm->isolate();
  Label probe_dictionary, property_array_property;
  // If we can load the value, it should be returned in x0.
  Register result = x0;

  GenerateKeyedLoadReceiverCheck(masm, receiver, scratch1, scratch2,
                                 Map::kHasNamedInterceptor, slow);

  // If the receiver is a fast-case object, check the stub cache. Otherwise
  // probe the dictionary.
  __ Ldr(scratch2, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ Ldr(scratch3, FieldMemOperand(scratch2, HeapObject::kMapOffset));
  __ JumpIfRoot(scratch3, Heap::kHashTableMapRootIndex, &probe_dictionary);

  // The handlers in the stub cache expect a vector and slot. Since we won't
  // change the IC from any downstream misses, a dummy vector can be used.
  Register vector = LoadWithVectorDescriptor::VectorRegister();
  Register slot = LoadWithVectorDescriptor::SlotRegister();
  DCHECK(!AreAliased(vector, slot, scratch1, scratch2, scratch3, scratch4));
  Handle<TypeFeedbackVector> dummy_vector =
      TypeFeedbackVector::DummyVector(masm->isolate());
  int slot_index = dummy_vector->GetIndex(
      FeedbackVectorSlot(TypeFeedbackVector::kDummyKeyedLoadICSlot));
  __ LoadRoot(vector, Heap::kDummyVectorRootIndex);
  __ Mov(slot, Operand(Smi::FromInt(slot_index)));

  Code::Flags flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::LOAD_IC));
  masm->isolate()->stub_cache()->GenerateProbe(masm, Code::KEYED_LOAD_IC, flags,
                                               receiver, key, scratch1,
                                               scratch2, scratch3, scratch4);
  // Cache miss.
  KeyedLoadIC::GenerateMiss(masm);

  // Do a quick inline probe of the receiver's dictionary, if it exists.
  __ Bind(&probe_dictionary);
  __ Ldr(scratch1, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ Ldrb(scratch1, FieldMemOperand(scratch1, Map::kInstanceTypeOffset));
  GenerateGlobalInstanceTypeCheck(masm, scratch1, slow);
  // Load the property.
  GenerateDictionaryLoad(masm, slow, scratch2, key, result, scratch1, scratch3);
  __ IncrementCounter(isolate->counters()->keyed_load_generic_symbol(), 1,
                      scratch1, scratch2);
  __ Ret();
}


void KeyedLoadIC::GenerateMegamorphic(MacroAssembler* masm,
                                      LanguageMode language_mode) {
  // The return address is in lr.
  Label slow, check_name, index_smi, index_name;

  Register key = LoadDescriptor::NameRegister();
  Register receiver = LoadDescriptor::ReceiverRegister();
  DCHECK(key.is(x2));
  DCHECK(receiver.is(x1));

  __ JumpIfNotSmi(key, &check_name);
  __ Bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from below
  // where a numeric string is converted to a smi.
  GenerateKeyedLoadWithSmiKey(masm, key, receiver, x7, x3, x4, x5, x6, &slow,
                              language_mode);

  // Slow case.
  __ Bind(&slow);
  __ IncrementCounter(masm->isolate()->counters()->keyed_load_generic_slow(), 1,
                      x4, x3);
  GenerateRuntimeGetProperty(masm, language_mode);

  __ Bind(&check_name);
  GenerateKeyNameCheck(masm, key, x0, x3, &index_name, &slow);

  GenerateKeyedLoadWithNameKey(masm, key, receiver, x4, x5, x6, x7, x3, &slow);

  __ Bind(&index_name);
  __ IndexFromHash(x3, key);
  // Now jump to the place where smi keys are handled.
  __ B(&index_smi);
}


static void StoreIC_PushArgs(MacroAssembler* masm) {
  __ Push(StoreDescriptor::ReceiverRegister(), StoreDescriptor::NameRegister(),
          StoreDescriptor::ValueRegister(),
          VectorStoreICDescriptor::SlotRegister(),
          VectorStoreICDescriptor::VectorRegister());
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm) {
  ASM_LOCATION("KeyedStoreIC::GenerateMiss");
  StoreIC_PushArgs(masm);
  __ TailCallRuntime(Runtime::kKeyedStoreIC_Miss);
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

  // Check that the receiver does not require access checks and is not observed.
  // The generic stub does not perform map checks or handle observed objects.
  __ Ldrb(x10, FieldMemOperand(receiver_map, Map::kBitFieldOffset));
  __ TestAndBranchIfAnySet(
      x10, (1 << Map::kIsAccessCheckNeeded) | (1 << Map::kIsObserved), &slow);

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
  Register vector = VectorStoreICDescriptor::VectorRegister();
  Register slot = VectorStoreICDescriptor::SlotRegister();
  DCHECK(!AreAliased(vector, slot, x5, x6, x7, x8));
  Handle<TypeFeedbackVector> dummy_vector =
      TypeFeedbackVector::DummyVector(masm->isolate());
  int slot_index = dummy_vector->GetIndex(
      FeedbackVectorSlot(TypeFeedbackVector::kDummyKeyedStoreICSlot));
  __ LoadRoot(vector, Heap::kDummyVectorRootIndex);
  __ Mov(slot, Operand(Smi::FromInt(slot_index)));

  Code::Flags flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::STORE_IC));
  masm->isolate()->stub_cache()->GenerateProbe(masm, Code::STORE_IC, flags,
                                               receiver, key, x5, x6, x7, x8);
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


void StoreIC::GenerateMegamorphic(MacroAssembler* masm) {
  Register receiver = StoreDescriptor::ReceiverRegister();
  Register name = StoreDescriptor::NameRegister();
  DCHECK(!AreAliased(receiver, name, StoreDescriptor::ValueRegister(), x3, x4,
                     x5, x6));

  // Probe the stub cache.
  Code::Flags flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::STORE_IC));
  masm->isolate()->stub_cache()->GenerateProbe(masm, Code::STORE_IC, flags,
                                               receiver, name, x3, x4, x5, x6);

  // Cache miss: Jump to runtime.
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
                     VectorStoreICDescriptor::SlotRegister(),
                     VectorStoreICDescriptor::VectorRegister(), x5, x6, x7));

  __ Ldr(dictionary, FieldMemOperand(receiver, JSObject::kPropertiesOffset));

  GenerateDictionaryStore(masm, &miss, dictionary, name, value, x6, x7);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->store_normal_hit(), 1, x6, x7);
  __ Ret();

  // Cache miss: Jump to runtime.
  __ Bind(&miss);
  __ IncrementCounter(counters->store_normal_miss(), 1, x6, x7);
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
    PrintF("[  Patching ic at %p, marker=%p, SMI check=%p\n", address,
           info_address, reinterpret_cast<void*>(info.SmiCheck()));
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
