// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/keyed-store-generic.h"

#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/contexts.h"
#include "src/ic/accessor-assembler-impl.h"
#include "src/interface-descriptors.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

using compiler::Node;

class KeyedStoreGenericAssembler : public AccessorAssemblerImpl {
 public:
  explicit KeyedStoreGenericAssembler(compiler::CodeAssemblerState* state)
      : AccessorAssemblerImpl(state) {}

  void KeyedStoreGeneric(LanguageMode language_mode);

 private:
  enum UpdateLength {
    kDontChangeLength,
    kIncrementLengthByOne,
    kBumpLengthWithGap
  };

  void EmitGenericElementStore(Node* receiver, Node* receiver_map,
                               Node* instance_type, Node* intptr_index,
                               Node* value, Node* context, Label* slow);

  void EmitGenericPropertyStore(Node* receiver, Node* receiver_map,
                                const StoreICParameters* p, Label* slow,
                                LanguageMode language_mode);

  void BranchIfPrototypesHaveNonFastElements(Node* receiver_map,
                                             Label* non_fast_elements,
                                             Label* only_fast_elements);

  void TryRewriteElements(Node* receiver, Node* receiver_map, Node* elements,
                          Node* native_context, ElementsKind from_kind,
                          ElementsKind to_kind, Label* bailout);

  void StoreElementWithCapacity(Node* receiver, Node* receiver_map,
                                Node* elements, Node* elements_kind,
                                Node* intptr_index, Node* value, Node* context,
                                Label* slow, UpdateLength update_length);

  void MaybeUpdateLengthAndReturn(Node* receiver, Node* index, Node* value,
                                  UpdateLength update_length);

  void TryChangeToHoleyMapHelper(Node* receiver, Node* receiver_map,
                                 Node* native_context, ElementsKind packed_kind,
                                 ElementsKind holey_kind, Label* done,
                                 Label* map_mismatch, Label* bailout);
  void TryChangeToHoleyMap(Node* receiver, Node* receiver_map,
                           Node* current_elements_kind, Node* context,
                           ElementsKind packed_kind, Label* bailout);
  void TryChangeToHoleyMapMulti(Node* receiver, Node* receiver_map,
                                Node* current_elements_kind, Node* context,
                                ElementsKind packed_kind,
                                ElementsKind packed_kind_2, Label* bailout);

  void JumpIfDataProperty(Node* details, Label* writable, Label* readonly);
  void LookupPropertyOnPrototypeChain(Node* receiver_map, Node* name,
                                      Label* accessor,
                                      Variable* var_accessor_pair,
                                      Variable* var_accessor_holder,
                                      Label* readonly, Label* bailout);
};

void KeyedStoreGenericGenerator::Generate(compiler::CodeAssemblerState* state,
                                          LanguageMode language_mode) {
  KeyedStoreGenericAssembler assembler(state);
  assembler.KeyedStoreGeneric(language_mode);
}

void KeyedStoreGenericAssembler::BranchIfPrototypesHaveNonFastElements(
    Node* receiver_map, Label* non_fast_elements, Label* only_fast_elements) {
  Variable var_map(this, MachineRepresentation::kTagged);
  var_map.Bind(receiver_map);
  Label loop_body(this, &var_map);
  Goto(&loop_body);

  Bind(&loop_body);
  {
    Node* map = var_map.value();
    Node* prototype = LoadMapPrototype(map);
    GotoIf(WordEqual(prototype, NullConstant()), only_fast_elements);
    Node* prototype_map = LoadMap(prototype);
    var_map.Bind(prototype_map);
    Node* instance_type = LoadMapInstanceType(prototype_map);
    STATIC_ASSERT(JS_PROXY_TYPE < JS_OBJECT_TYPE);
    STATIC_ASSERT(JS_VALUE_TYPE < JS_OBJECT_TYPE);
    GotoIf(Int32LessThanOrEqual(instance_type,
                                Int32Constant(LAST_CUSTOM_ELEMENTS_RECEIVER)),
           non_fast_elements);
    Node* elements_kind = LoadMapElementsKind(prototype_map);
    STATIC_ASSERT(FIRST_ELEMENTS_KIND == FIRST_FAST_ELEMENTS_KIND);
    GotoIf(IsFastElementsKind(elements_kind), &loop_body);
    GotoIf(Word32Equal(elements_kind, Int32Constant(NO_ELEMENTS)), &loop_body);
    Goto(non_fast_elements);
  }
}

void KeyedStoreGenericAssembler::TryRewriteElements(
    Node* receiver, Node* receiver_map, Node* elements, Node* native_context,
    ElementsKind from_kind, ElementsKind to_kind, Label* bailout) {
  DCHECK(IsFastPackedElementsKind(from_kind));
  ElementsKind holey_from_kind = GetHoleyElementsKind(from_kind);
  ElementsKind holey_to_kind = GetHoleyElementsKind(to_kind);
  if (AllocationSite::GetMode(from_kind, to_kind) == TRACK_ALLOCATION_SITE) {
    TrapAllocationMemento(receiver, bailout);
  }
  Label perform_transition(this), check_holey_map(this);
  Variable var_target_map(this, MachineRepresentation::kTagged);
  // Check if the receiver has the default |from_kind| map.
  {
    Node* packed_map =
        LoadContextElement(native_context, Context::ArrayMapIndex(from_kind));
    GotoIf(WordNotEqual(receiver_map, packed_map), &check_holey_map);
    var_target_map.Bind(
        LoadContextElement(native_context, Context::ArrayMapIndex(to_kind)));
    Goto(&perform_transition);
  }

  // Check if the receiver has the default |holey_from_kind| map.
  Bind(&check_holey_map);
  {
    Node* holey_map = LoadContextElement(
        native_context, Context::ArrayMapIndex(holey_from_kind));
    GotoIf(WordNotEqual(receiver_map, holey_map), bailout);
    var_target_map.Bind(LoadContextElement(
        native_context, Context::ArrayMapIndex(holey_to_kind)));
    Goto(&perform_transition);
  }

  // Found a supported transition target map, perform the transition!
  Bind(&perform_transition);
  {
    if (IsFastDoubleElementsKind(from_kind) !=
        IsFastDoubleElementsKind(to_kind)) {
      Node* capacity = SmiUntag(LoadFixedArrayBaseLength(elements));
      GrowElementsCapacity(receiver, elements, from_kind, to_kind, capacity,
                           capacity, INTPTR_PARAMETERS, bailout);
    }
    StoreMap(receiver, var_target_map.value());
  }
}

void KeyedStoreGenericAssembler::TryChangeToHoleyMapHelper(
    Node* receiver, Node* receiver_map, Node* native_context,
    ElementsKind packed_kind, ElementsKind holey_kind, Label* done,
    Label* map_mismatch, Label* bailout) {
  Node* packed_map =
      LoadContextElement(native_context, Context::ArrayMapIndex(packed_kind));
  GotoIf(WordNotEqual(receiver_map, packed_map), map_mismatch);
  if (AllocationSite::GetMode(packed_kind, holey_kind) ==
      TRACK_ALLOCATION_SITE) {
    TrapAllocationMemento(receiver, bailout);
  }
  Node* holey_map =
      LoadContextElement(native_context, Context::ArrayMapIndex(holey_kind));
  StoreMap(receiver, holey_map);
  Goto(done);
}

void KeyedStoreGenericAssembler::TryChangeToHoleyMap(
    Node* receiver, Node* receiver_map, Node* current_elements_kind,
    Node* context, ElementsKind packed_kind, Label* bailout) {
  ElementsKind holey_kind = GetHoleyElementsKind(packed_kind);
  Label already_holey(this);

  GotoIf(Word32Equal(current_elements_kind, Int32Constant(holey_kind)),
         &already_holey);
  Node* native_context = LoadNativeContext(context);
  TryChangeToHoleyMapHelper(receiver, receiver_map, native_context, packed_kind,
                            holey_kind, &already_holey, bailout, bailout);
  Bind(&already_holey);
}

void KeyedStoreGenericAssembler::TryChangeToHoleyMapMulti(
    Node* receiver, Node* receiver_map, Node* current_elements_kind,
    Node* context, ElementsKind packed_kind, ElementsKind packed_kind_2,
    Label* bailout) {
  ElementsKind holey_kind = GetHoleyElementsKind(packed_kind);
  ElementsKind holey_kind_2 = GetHoleyElementsKind(packed_kind_2);
  Label already_holey(this), check_other_kind(this);

  GotoIf(Word32Equal(current_elements_kind, Int32Constant(holey_kind)),
         &already_holey);
  GotoIf(Word32Equal(current_elements_kind, Int32Constant(holey_kind_2)),
         &already_holey);

  Node* native_context = LoadNativeContext(context);
  TryChangeToHoleyMapHelper(receiver, receiver_map, native_context, packed_kind,
                            holey_kind, &already_holey, &check_other_kind,
                            bailout);
  Bind(&check_other_kind);
  TryChangeToHoleyMapHelper(receiver, receiver_map, native_context,
                            packed_kind_2, holey_kind_2, &already_holey,
                            bailout, bailout);
  Bind(&already_holey);
}

void KeyedStoreGenericAssembler::MaybeUpdateLengthAndReturn(
    Node* receiver, Node* index, Node* value, UpdateLength update_length) {
  if (update_length != kDontChangeLength) {
    Node* new_length = SmiTag(IntPtrAdd(index, IntPtrConstant(1)));
    StoreObjectFieldNoWriteBarrier(receiver, JSArray::kLengthOffset, new_length,
                                   MachineRepresentation::kTagged);
  }
  Return(value);
}

void KeyedStoreGenericAssembler::StoreElementWithCapacity(
    Node* receiver, Node* receiver_map, Node* elements, Node* elements_kind,
    Node* intptr_index, Node* value, Node* context, Label* slow,
    UpdateLength update_length) {
  if (update_length != kDontChangeLength) {
    CSA_ASSERT(this, Word32Equal(LoadMapInstanceType(receiver_map),
                                 Int32Constant(JS_ARRAY_TYPE)));
    // Check if the length property is writable. The fast check is only
    // supported for fast properties.
    GotoIf(IsDictionaryMap(receiver_map), slow);
    // The length property is non-configurable, so it's guaranteed to always
    // be the first property.
    Node* descriptors = LoadMapDescriptors(receiver_map);
    Node* details =
        LoadFixedArrayElement(descriptors, DescriptorArray::ToDetailsIndex(0));
    GotoIf(IsSetSmi(details, PropertyDetails::kAttributesReadOnlyMask), slow);
  }
  STATIC_ASSERT(FixedArray::kHeaderSize == FixedDoubleArray::kHeaderSize);
  const int kHeaderSize = FixedArray::kHeaderSize - kHeapObjectTag;

  Label check_double_elements(this), check_cow_elements(this);
  Node* elements_map = LoadMap(elements);
  GotoIf(WordNotEqual(elements_map, LoadRoot(Heap::kFixedArrayMapRootIndex)),
         &check_double_elements);

  // FixedArray backing store -> Smi or object elements.
  {
    Node* offset = ElementOffsetFromIndex(intptr_index, FAST_ELEMENTS,
                                          INTPTR_PARAMETERS, kHeaderSize);
    // Check if we're about to overwrite the hole. We can safely do that
    // only if there can be no setters on the prototype chain.
    // If we know that we're storing beyond the previous array length, we
    // can skip the hole check (and always assume the hole).
    {
      Label hole_check_passed(this);
      if (update_length == kDontChangeLength) {
        Node* element = Load(MachineType::AnyTagged(), elements, offset);
        GotoIf(WordNotEqual(element, TheHoleConstant()), &hole_check_passed);
      }
      BranchIfPrototypesHaveNonFastElements(receiver_map, slow,
                                            &hole_check_passed);
      Bind(&hole_check_passed);
    }

    // Check if the value we're storing matches the elements_kind. Smis
    // can always be stored.
    {
      Label non_smi_value(this);
      GotoUnless(TaggedIsSmi(value), &non_smi_value);
      // If we're about to introduce holes, ensure holey elements.
      if (update_length == kBumpLengthWithGap) {
        TryChangeToHoleyMapMulti(receiver, receiver_map, elements_kind, context,
                                 FAST_SMI_ELEMENTS, FAST_ELEMENTS, slow);
      }
      StoreNoWriteBarrier(MachineRepresentation::kTagged, elements, offset,
                          value);
      MaybeUpdateLengthAndReturn(receiver, intptr_index, value, update_length);

      Bind(&non_smi_value);
    }

    // Check if we already have object elements; just do the store if so.
    {
      Label must_transition(this);
      STATIC_ASSERT(FAST_SMI_ELEMENTS == 0);
      STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS == 1);
      GotoIf(Int32LessThanOrEqual(elements_kind,
                                  Int32Constant(FAST_HOLEY_SMI_ELEMENTS)),
             &must_transition);
      if (update_length == kBumpLengthWithGap) {
        TryChangeToHoleyMap(receiver, receiver_map, elements_kind, context,
                            FAST_ELEMENTS, slow);
      }
      Store(elements, offset, value);
      MaybeUpdateLengthAndReturn(receiver, intptr_index, value, update_length);

      Bind(&must_transition);
    }

    // Transition to the required ElementsKind.
    {
      Label transition_to_double(this), transition_to_object(this);
      Node* native_context = LoadNativeContext(context);
      Branch(WordEqual(LoadMap(value), LoadRoot(Heap::kHeapNumberMapRootIndex)),
             &transition_to_double, &transition_to_object);
      Bind(&transition_to_double);
      {
        // If we're adding holes at the end, always transition to a holey
        // elements kind, otherwise try to remain packed.
        ElementsKind target_kind = update_length == kBumpLengthWithGap
                                       ? FAST_HOLEY_DOUBLE_ELEMENTS
                                       : FAST_DOUBLE_ELEMENTS;
        TryRewriteElements(receiver, receiver_map, elements, native_context,
                           FAST_SMI_ELEMENTS, target_kind, slow);
        // Reload migrated elements.
        Node* double_elements = LoadElements(receiver);
        Node* double_offset = ElementOffsetFromIndex(
            intptr_index, FAST_DOUBLE_ELEMENTS, INTPTR_PARAMETERS, kHeaderSize);
        // Make sure we do not store signalling NaNs into double arrays.
        Node* double_value = Float64SilenceNaN(LoadHeapNumberValue(value));
        StoreNoWriteBarrier(MachineRepresentation::kFloat64, double_elements,
                            double_offset, double_value);
        MaybeUpdateLengthAndReturn(receiver, intptr_index, value,
                                   update_length);
      }

      Bind(&transition_to_object);
      {
        // If we're adding holes at the end, always transition to a holey
        // elements kind, otherwise try to remain packed.
        ElementsKind target_kind = update_length == kBumpLengthWithGap
                                       ? FAST_HOLEY_ELEMENTS
                                       : FAST_ELEMENTS;
        TryRewriteElements(receiver, receiver_map, elements, native_context,
                           FAST_SMI_ELEMENTS, target_kind, slow);
        // The elements backing store didn't change, no reload necessary.
        CSA_ASSERT(this, WordEqual(elements, LoadElements(receiver)));
        Store(elements, offset, value);
        MaybeUpdateLengthAndReturn(receiver, intptr_index, value,
                                   update_length);
      }
    }
  }

  Bind(&check_double_elements);
  Node* fixed_double_array_map = LoadRoot(Heap::kFixedDoubleArrayMapRootIndex);
  GotoIf(WordNotEqual(elements_map, fixed_double_array_map),
         &check_cow_elements);
  // FixedDoubleArray backing store -> double elements.
  {
    Node* offset = ElementOffsetFromIndex(intptr_index, FAST_DOUBLE_ELEMENTS,
                                          INTPTR_PARAMETERS, kHeaderSize);
    // Check if we're about to overwrite the hole. We can safely do that
    // only if there can be no setters on the prototype chain.
    {
      Label hole_check_passed(this);
      // If we know that we're storing beyond the previous array length, we
      // can skip the hole check (and always assume the hole).
      if (update_length == kDontChangeLength) {
        Label found_hole(this);
        LoadDoubleWithHoleCheck(elements, offset, &found_hole,
                                MachineType::None());
        Goto(&hole_check_passed);
        Bind(&found_hole);
      }
      BranchIfPrototypesHaveNonFastElements(receiver_map, slow,
                                            &hole_check_passed);
      Bind(&hole_check_passed);
    }

    // Try to store the value as a double.
    {
      Label non_number_value(this);
      Node* double_value = TryTaggedToFloat64(value, &non_number_value);

      // Make sure we do not store signalling NaNs into double arrays.
      double_value = Float64SilenceNaN(double_value);
      // If we're about to introduce holes, ensure holey elements.
      if (update_length == kBumpLengthWithGap) {
        TryChangeToHoleyMap(receiver, receiver_map, elements_kind, context,
                            FAST_DOUBLE_ELEMENTS, slow);
      }
      StoreNoWriteBarrier(MachineRepresentation::kFloat64, elements, offset,
                          double_value);
      MaybeUpdateLengthAndReturn(receiver, intptr_index, value, update_length);

      Bind(&non_number_value);
    }

    // Transition to object elements.
    {
      Node* native_context = LoadNativeContext(context);
      ElementsKind target_kind = update_length == kBumpLengthWithGap
                                     ? FAST_HOLEY_ELEMENTS
                                     : FAST_ELEMENTS;
      TryRewriteElements(receiver, receiver_map, elements, native_context,
                         FAST_DOUBLE_ELEMENTS, target_kind, slow);
      // Reload migrated elements.
      Node* fast_elements = LoadElements(receiver);
      Node* fast_offset = ElementOffsetFromIndex(
          intptr_index, FAST_ELEMENTS, INTPTR_PARAMETERS, kHeaderSize);
      Store(fast_elements, fast_offset, value);
      MaybeUpdateLengthAndReturn(receiver, intptr_index, value, update_length);
    }
  }

  Bind(&check_cow_elements);
  {
    // TODO(jkummerow): Use GrowElementsCapacity instead of bailing out.
    Goto(slow);
  }
}

void KeyedStoreGenericAssembler::EmitGenericElementStore(
    Node* receiver, Node* receiver_map, Node* instance_type, Node* intptr_index,
    Node* value, Node* context, Label* slow) {
  Label if_fast(this), if_in_bounds(this), if_increment_length_by_one(this),
      if_bump_length_with_gap(this), if_grow(this), if_nonfast(this),
      if_typed_array(this), if_dictionary(this);
  Node* elements = LoadElements(receiver);
  Node* elements_kind = LoadMapElementsKind(receiver_map);
  Branch(IsFastElementsKind(elements_kind), &if_fast, &if_nonfast);
  Bind(&if_fast);

  Label if_array(this);
  GotoIf(Word32Equal(instance_type, Int32Constant(JS_ARRAY_TYPE)), &if_array);
  {
    Node* capacity = SmiUntag(LoadFixedArrayBaseLength(elements));
    Branch(UintPtrLessThan(intptr_index, capacity), &if_in_bounds, &if_grow);
  }
  Bind(&if_array);
  {
    Node* length = SmiUntag(LoadJSArrayLength(receiver));
    GotoIf(UintPtrLessThan(intptr_index, length), &if_in_bounds);
    Node* capacity = SmiUntag(LoadFixedArrayBaseLength(elements));
    GotoIf(UintPtrGreaterThanOrEqual(intptr_index, capacity), &if_grow);
    Branch(WordEqual(intptr_index, length), &if_increment_length_by_one,
           &if_bump_length_with_gap);
  }

  Bind(&if_in_bounds);
  {
    StoreElementWithCapacity(receiver, receiver_map, elements, elements_kind,
                             intptr_index, value, context, slow,
                             kDontChangeLength);
  }

  Bind(&if_increment_length_by_one);
  {
    StoreElementWithCapacity(receiver, receiver_map, elements, elements_kind,
                             intptr_index, value, context, slow,
                             kIncrementLengthByOne);
  }

  Bind(&if_bump_length_with_gap);
  {
    StoreElementWithCapacity(receiver, receiver_map, elements, elements_kind,
                             intptr_index, value, context, slow,
                             kBumpLengthWithGap);
  }

  // Out-of-capacity accesses (index >= capacity) jump here. Additionally,
  // an ElementsKind transition might be necessary.
  Bind(&if_grow);
  {
    Comment("Grow backing store");
    // TODO(jkummerow): Support inline backing store growth.
    Goto(slow);
  }

  // Any ElementsKind > LAST_FAST_ELEMENTS_KIND jumps here for further dispatch.
  Bind(&if_nonfast);
  {
    STATIC_ASSERT(LAST_ELEMENTS_KIND == LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND);
    GotoIf(Int32GreaterThanOrEqual(
               elements_kind,
               Int32Constant(FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND)),
           &if_typed_array);
    GotoIf(Word32Equal(elements_kind, Int32Constant(DICTIONARY_ELEMENTS)),
           &if_dictionary);
    Goto(slow);
  }

  Bind(&if_dictionary);
  {
    Comment("Dictionary");
    // TODO(jkummerow): Support storing to dictionary elements.
    Goto(slow);
  }

  Bind(&if_typed_array);
  {
    Comment("Typed array");
    // TODO(jkummerow): Support typed arrays.
    Goto(slow);
  }
}

void KeyedStoreGenericAssembler::JumpIfDataProperty(Node* details,
                                                    Label* writable,
                                                    Label* readonly) {
  // Accessor properties never have the READ_ONLY attribute set.
  GotoIf(IsSetWord32(details, PropertyDetails::kAttributesReadOnlyMask),
         readonly);
  Node* kind = DecodeWord32<PropertyDetails::KindField>(details);
  GotoIf(Word32Equal(kind, Int32Constant(kData)), writable);
  // Fall through if it's an accessor property.
}

void KeyedStoreGenericAssembler::LookupPropertyOnPrototypeChain(
    Node* receiver_map, Node* name, Label* accessor,
    Variable* var_accessor_pair, Variable* var_accessor_holder, Label* readonly,
    Label* bailout) {
  Label ok_to_write(this);
  Variable var_holder(this, MachineRepresentation::kTagged);
  var_holder.Bind(LoadMapPrototype(receiver_map));
  Variable var_holder_map(this, MachineRepresentation::kTagged);
  var_holder_map.Bind(LoadMap(var_holder.value()));

  Variable* merged_variables[] = {&var_holder, &var_holder_map};
  Label loop(this, arraysize(merged_variables), merged_variables);
  Goto(&loop);
  Bind(&loop);
  {
    Node* holder = var_holder.value();
    Node* holder_map = var_holder_map.value();
    Node* instance_type = LoadMapInstanceType(holder_map);
    Label next_proto(this);
    {
      Label found(this), found_fast(this), found_dict(this), found_global(this);
      Variable var_meta_storage(this, MachineRepresentation::kTagged);
      Variable var_entry(this, MachineType::PointerRepresentation());
      TryLookupProperty(holder, holder_map, instance_type, name, &found_fast,
                        &found_dict, &found_global, &var_meta_storage,
                        &var_entry, &next_proto, bailout);
      Bind(&found_fast);
      {
        Node* descriptors = var_meta_storage.value();
        Node* name_index = var_entry.value();
        // TODO(jkummerow): Add helper functions for accessing value and
        // details by entry.
        const int kNameToDetailsOffset = (DescriptorArray::kDescriptorDetails -
                                          DescriptorArray::kDescriptorKey) *
                                         kPointerSize;
        Node* details = LoadAndUntagToWord32FixedArrayElement(
            descriptors, name_index, kNameToDetailsOffset);
        JumpIfDataProperty(details, &ok_to_write, readonly);

        // Accessor case.
        Variable var_details(this, MachineRepresentation::kWord32);
        LoadPropertyFromFastObject(holder, holder_map, descriptors, name_index,
                                   &var_details, var_accessor_pair);
        var_accessor_holder->Bind(holder);
        Goto(accessor);
      }

      Bind(&found_dict);
      {
        Node* dictionary = var_meta_storage.value();
        Node* entry = var_entry.value();
        const int kNameToDetailsOffset = (NameDictionary::kEntryDetailsIndex -
                                          NameDictionary::kEntryKeyIndex) *
                                         kPointerSize;
        Node* details = LoadAndUntagToWord32FixedArrayElement(
            dictionary, entry, kNameToDetailsOffset);
        JumpIfDataProperty(details, &ok_to_write, readonly);

        // Accessor case.
        const int kNameToValueOffset = (NameDictionary::kEntryValueIndex -
                                        NameDictionary::kEntryKeyIndex) *
                                       kPointerSize;
        var_accessor_pair->Bind(
            LoadFixedArrayElement(dictionary, entry, kNameToValueOffset));
        var_accessor_holder->Bind(holder);
        Goto(accessor);
      }

      Bind(&found_global);
      {
        Node* dictionary = var_meta_storage.value();
        Node* entry = var_entry.value();
        const int kNameToValueOffset = (GlobalDictionary::kEntryValueIndex -
                                        GlobalDictionary::kEntryKeyIndex) *
                                       kPointerSize;

        Node* property_cell =
            LoadFixedArrayElement(dictionary, entry, kNameToValueOffset);

        Node* value =
            LoadObjectField(property_cell, PropertyCell::kValueOffset);
        GotoIf(WordEqual(value, TheHoleConstant()), &next_proto);
        Node* details = LoadAndUntagToWord32ObjectField(
            property_cell, PropertyCell::kDetailsOffset);
        JumpIfDataProperty(details, &ok_to_write, readonly);

        // Accessor case.
        var_accessor_pair->Bind(value);
        var_accessor_holder->Bind(holder);
        Goto(accessor);
      }
    }

    Bind(&next_proto);
    // Bailout if it can be an integer indexed exotic case.
    GotoIf(Word32Equal(instance_type, Int32Constant(JS_TYPED_ARRAY_TYPE)),
           bailout);
    Node* proto = LoadMapPrototype(holder_map);
    GotoIf(WordEqual(proto, NullConstant()), &ok_to_write);
    var_holder.Bind(proto);
    var_holder_map.Bind(LoadMap(proto));
    Goto(&loop);
  }
  Bind(&ok_to_write);
}

void KeyedStoreGenericAssembler::EmitGenericPropertyStore(
    Node* receiver, Node* receiver_map, const StoreICParameters* p, Label* slow,
    LanguageMode language_mode) {
  Variable var_accessor_pair(this, MachineRepresentation::kTagged);
  Variable var_accessor_holder(this, MachineRepresentation::kTagged);
  Label stub_cache(this), fast_properties(this), dictionary_properties(this),
      accessor(this), readonly(this);
  Node* properties = LoadProperties(receiver);
  Node* properties_map = LoadMap(properties);
  Branch(WordEqual(properties_map, LoadRoot(Heap::kHashTableMapRootIndex)),
         &dictionary_properties, &fast_properties);

  Bind(&fast_properties);
  {
    // TODO(jkummerow): Does it make sense to support some cases here inline?
    // Maybe overwrite existing writable properties?
    // Maybe support map transitions?
    Goto(&stub_cache);
  }

  Bind(&dictionary_properties);
  {
    Comment("dictionary property store");
    // We checked for LAST_CUSTOM_ELEMENTS_RECEIVER before, which rules out
    // seeing global objects here (which would need special handling).

    Variable var_name_index(this, MachineType::PointerRepresentation());
    Label dictionary_found(this, &var_name_index), not_found(this);
    NameDictionaryLookup<NameDictionary>(properties, p->name, &dictionary_found,
                                         &var_name_index, &not_found);
    Bind(&dictionary_found);
    {
      Label overwrite(this);
      const int kNameToDetailsOffset = (NameDictionary::kEntryDetailsIndex -
                                        NameDictionary::kEntryKeyIndex) *
                                       kPointerSize;
      Node* details = LoadAndUntagToWord32FixedArrayElement(
          properties, var_name_index.value(), kNameToDetailsOffset);
      JumpIfDataProperty(details, &overwrite, &readonly);

      // Accessor case.
      const int kNameToValueOffset =
          (NameDictionary::kEntryValueIndex - NameDictionary::kEntryKeyIndex) *
          kPointerSize;
      var_accessor_pair.Bind(LoadFixedArrayElement(
          properties, var_name_index.value(), kNameToValueOffset));
      var_accessor_holder.Bind(receiver);
      Goto(&accessor);

      Bind(&overwrite);
      {
        StoreFixedArrayElement(properties, var_name_index.value(), p->value,
                               UPDATE_WRITE_BARRIER, kNameToValueOffset);
        Return(p->value);
      }
    }

    Bind(&not_found);
    {
      LookupPropertyOnPrototypeChain(receiver_map, p->name, &accessor,
                                     &var_accessor_pair, &var_accessor_holder,
                                     &readonly, slow);
      Add<NameDictionary>(properties, p->name, p->value, slow);
      Return(p->value);
    }
  }

  Bind(&accessor);
  {
    Label not_callable(this);
    Node* accessor_pair = var_accessor_pair.value();
    GotoIf(IsAccessorInfoMap(LoadMap(accessor_pair)), slow);
    CSA_ASSERT(this, HasInstanceType(accessor_pair, ACCESSOR_PAIR_TYPE));
    Node* setter = LoadObjectField(accessor_pair, AccessorPair::kSetterOffset);
    Node* setter_map = LoadMap(setter);
    // FunctionTemplateInfo setters are not supported yet.
    GotoIf(IsFunctionTemplateInfoMap(setter_map), slow);
    GotoUnless(IsCallableMap(setter_map), &not_callable);

    Callable callable = CodeFactory::Call(isolate());
    CallJS(callable, p->context, setter, receiver, p->value);
    Return(p->value);

    Bind(&not_callable);
    {
      if (language_mode == STRICT) {
        Node* message =
            SmiConstant(Smi::FromInt(MessageTemplate::kNoSetterInCallback));
        TailCallRuntime(Runtime::kThrowTypeError, p->context, message, p->name,
                        var_accessor_holder.value());
      } else {
        DCHECK_EQ(SLOPPY, language_mode);
        Return(p->value);
      }
    }
  }

  Bind(&readonly);
  {
    if (language_mode == STRICT) {
      Node* message =
          SmiConstant(Smi::FromInt(MessageTemplate::kStrictReadOnlyProperty));
      Node* type = Typeof(p->receiver, p->context);
      TailCallRuntime(Runtime::kThrowTypeError, p->context, message, p->name,
                      type, p->receiver);
    } else {
      DCHECK_EQ(SLOPPY, language_mode);
      Return(p->value);
    }
  }

  Bind(&stub_cache);
  {
    Comment("stub cache probe");
    Variable var_handler(this, MachineRepresentation::kTagged);
    Label found_handler(this, &var_handler), stub_cache_miss(this);
    TryProbeStubCache(isolate()->store_stub_cache(), receiver, p->name,
                      &found_handler, &var_handler, &stub_cache_miss);
    Bind(&found_handler);
    {
      Comment("KeyedStoreGeneric found handler");
      HandleStoreICHandlerCase(p, var_handler.value(), slow);
    }
    Bind(&stub_cache_miss);
    {
      Comment("KeyedStoreGeneric_miss");
      TailCallRuntime(Runtime::kKeyedStoreIC_Miss, p->context, p->value,
                      p->slot, p->vector, p->receiver, p->name);
    }
  }
}

void KeyedStoreGenericAssembler::KeyedStoreGeneric(LanguageMode language_mode) {
  typedef StoreWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Variable var_index(this, MachineType::PointerRepresentation());
  Label if_index(this), if_unique_name(this), slow(this);

  GotoIf(TaggedIsSmi(receiver), &slow);
  Node* receiver_map = LoadMap(receiver);
  Node* instance_type = LoadMapInstanceType(receiver_map);
  // Receivers requiring non-standard element accesses (interceptors, access
  // checks, strings and string wrappers, proxies) are handled in the runtime.
  GotoIf(Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_CUSTOM_ELEMENTS_RECEIVER)),
         &slow);

  TryToName(name, &if_index, &var_index, &if_unique_name, &slow);

  Bind(&if_index);
  {
    Comment("integer index");
    EmitGenericElementStore(receiver, receiver_map, instance_type,
                            var_index.value(), value, context, &slow);
  }

  Bind(&if_unique_name);
  {
    Comment("key is unique name");
    KeyedStoreGenericAssembler::StoreICParameters p(context, receiver, name,
                                                    value, slot, vector);
    EmitGenericPropertyStore(receiver, receiver_map, &p, &slow, language_mode);
  }

  Bind(&slow);
  {
    Comment("KeyedStoreGeneric_slow");
    TailCallRuntime(Runtime::kSetProperty, context, receiver, name, value,
                    SmiConstant(language_mode));
  }
}

}  // namespace internal
}  // namespace v8
