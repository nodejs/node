// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/keyed-store-generic.h"

#include "src/compiler/code-assembler.h"
#include "src/contexts.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {

using compiler::Node;

class KeyedStoreGenericAssembler : public CodeStubAssembler {
 public:
  void KeyedStoreGeneric(const StoreICParameters* p,
                         LanguageMode language_mode);

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
                                const StoreICParameters* p, Label* slow);

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

  // Do not add fields, so that this is safe to reinterpret_cast to CSA.
};

void KeyedStoreGenericGenerator::Generate(
    CodeStubAssembler* assembler, const CodeStubAssembler::StoreICParameters* p,
    LanguageMode language_mode) {
  STATIC_ASSERT(sizeof(CodeStubAssembler) ==
                sizeof(KeyedStoreGenericAssembler));
  auto assm = reinterpret_cast<KeyedStoreGenericAssembler*>(assembler);
  assm->KeyedStoreGeneric(p, language_mode);
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
    GotoIf(Int32LessThanOrEqual(elements_kind,
                                Int32Constant(LAST_FAST_ELEMENTS_KIND)),
           &loop_body);
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
  Variable var_target_map(this, MachineType::PointerRepresentation());
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
    StoreObjectField(receiver, JSObject::kMapOffset, var_target_map.value());
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
  StoreObjectField(receiver, JSObject::kMapOffset, holey_map);
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
      Store(MachineRepresentation::kTagged, elements, offset, value);
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
        Store(MachineRepresentation::kTagged, elements, offset, value);
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
      Node* double_value = PrepareValueForWrite(value, Representation::Double(),
                                                &non_number_value);
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
      Store(MachineRepresentation::kTagged, fast_elements, fast_offset, value);
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
  Label if_in_bounds(this), if_increment_length_by_one(this),
      if_bump_length_with_gap(this), if_grow(this), if_nonfast(this),
      if_typed_array(this), if_dictionary(this);
  Node* elements = LoadElements(receiver);
  Node* elements_kind = LoadMapElementsKind(receiver_map);
  GotoIf(
      Int32GreaterThan(elements_kind, Int32Constant(LAST_FAST_ELEMENTS_KIND)),
      &if_nonfast);

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

void KeyedStoreGenericAssembler::EmitGenericPropertyStore(
    Node* receiver, Node* receiver_map, const StoreICParameters* p,
    Label* slow) {
  Comment("stub cache probe");
  // TODO(jkummerow): Don't rely on the stub cache as much.
  // - existing properties can be overwritten inline (unless readonly).
  // - for dictionary mode receivers, we can even add properties inline
  //   (unless the prototype chain prevents it).
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
    TailCallRuntime(Runtime::kKeyedStoreIC_Miss, p->context, p->value, p->slot,
                    p->vector, p->receiver, p->name);
  }
}

void KeyedStoreGenericAssembler::KeyedStoreGeneric(const StoreICParameters* p,
                                                   LanguageMode language_mode) {
  Variable var_index(this, MachineType::PointerRepresentation());
  Label if_index(this), if_unique_name(this), slow(this);

  Node* receiver = p->receiver;
  GotoIf(TaggedIsSmi(receiver), &slow);
  Node* receiver_map = LoadMap(receiver);
  Node* instance_type = LoadMapInstanceType(receiver_map);
  // Receivers requiring non-standard element accesses (interceptors, access
  // checks, strings and string wrappers, proxies) are handled in the runtime.
  GotoIf(Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_CUSTOM_ELEMENTS_RECEIVER)),
         &slow);

  TryToName(p->name, &if_index, &var_index, &if_unique_name, &slow);

  Bind(&if_index);
  {
    Comment("integer index");
    EmitGenericElementStore(receiver, receiver_map, instance_type,
                            var_index.value(), p->value, p->context, &slow);
  }

  Bind(&if_unique_name);
  {
    Comment("key is unique name");
    EmitGenericPropertyStore(receiver, receiver_map, p, &slow);
  }

  Bind(&slow);
  {
    Comment("KeyedStoreGeneric_slow");
    TailCallRuntime(Runtime::kSetProperty, p->context, p->receiver, p->name,
                    p->value, SmiConstant(language_mode));
  }
}

}  // namespace internal
}  // namespace v8
