// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/keyed-store-generic.h"

#include "src/codegen/code-factory.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/codegen/interface-descriptors.h"
#include "src/execution/isolate.h"
#include "src/ic/accessor-assembler.h"
#include "src/objects/contexts.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

using Node = compiler::Node;
template <class T>
using TNode = compiler::TNode<T>;

enum class StoreMode { kOrdinary, kInLiteral };

class KeyedStoreGenericAssembler : public AccessorAssembler {
 public:
  explicit KeyedStoreGenericAssembler(compiler::CodeAssemblerState* state,
                                      StoreMode mode)
      : AccessorAssembler(state), mode_(mode) {}

  void KeyedStoreGeneric();

  void StoreIC_Uninitialized();

  // Generates code for [[Set]] operation, the |unique_name| is supposed to be
  // unique otherwise this code will always go to runtime.
  void SetProperty(TNode<Context> context, TNode<JSReceiver> receiver,
                   TNode<BoolT> is_simple_receiver, TNode<Name> unique_name,
                   TNode<Object> value, LanguageMode language_mode);

  // [[Set]], but more generic than the above. This impl does essentially the
  // same as "KeyedStoreGeneric" but does not use feedback slot and uses a
  // hardcoded LanguageMode instead of trying to deduce it from the feedback
  // slot's kind.
  void SetProperty(TNode<Context> context, TNode<Object> receiver,
                   TNode<Object> key, TNode<Object> value,
                   LanguageMode language_mode);

 private:
  StoreMode mode_;

  enum UpdateLength {
    kDontChangeLength,
    kIncrementLengthByOne,
    kBumpLengthWithGap
  };

  enum UseStubCache { kUseStubCache, kDontUseStubCache };

  // Helper that is used by the public KeyedStoreGeneric and by SetProperty.
  void KeyedStoreGeneric(TNode<Context> context, TNode<Object> receiver,
                         TNode<Object> key, TNode<Object> value,
                         Maybe<LanguageMode> language_mode);

  void EmitGenericElementStore(Node* receiver, Node* receiver_map,
                               Node* instance_type, Node* intptr_index,
                               Node* value, Node* context, Label* slow);

  // If language mode is not provided it is deduced from the feedback slot's
  // kind.
  void EmitGenericPropertyStore(TNode<JSReceiver> receiver,
                                TNode<Map> receiver_map,
                                const StoreICParameters* p,
                                ExitPoint* exit_point, Label* slow,
                                Maybe<LanguageMode> maybe_language_mode);

  void EmitGenericPropertyStore(SloppyTNode<JSReceiver> receiver,
                                SloppyTNode<Map> receiver_map,
                                const StoreICParameters* p, Label* slow) {
    ExitPoint direct_exit(this);
    EmitGenericPropertyStore(receiver, receiver_map, p, &direct_exit, slow,
                             Nothing<LanguageMode>());
  }

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

  void LookupPropertyOnPrototypeChain(Node* receiver_map, Node* name,
                                      Label* accessor,
                                      Variable* var_accessor_pair,
                                      Variable* var_accessor_holder,
                                      Label* readonly, Label* bailout);

  TNode<Map> FindCandidateStoreICTransitionMapHandler(TNode<Map> map,
                                                      TNode<Name> name,
                                                      Label* slow);

  bool IsKeyedStore() const { return mode_ == StoreMode::kOrdinary; }
  bool IsStoreInLiteral() const { return mode_ == StoreMode::kInLiteral; }

  bool ShouldCheckPrototype() const { return IsKeyedStore(); }
  bool ShouldReconfigureExisting() const { return IsStoreInLiteral(); }
  bool ShouldCallSetter() const { return IsKeyedStore(); }
  bool ShouldCheckPrototypeValidity() const {
    // We don't do this for "in-literal" stores, because it is impossible for
    // the target object to be a "prototype"
    return !IsStoreInLiteral();
  }
};

void KeyedStoreGenericGenerator::Generate(compiler::CodeAssemblerState* state) {
  KeyedStoreGenericAssembler assembler(state, StoreMode::kOrdinary);
  assembler.KeyedStoreGeneric();
}

void StoreICUninitializedGenerator::Generate(
    compiler::CodeAssemblerState* state) {
  KeyedStoreGenericAssembler assembler(state, StoreMode::kOrdinary);
  assembler.StoreIC_Uninitialized();
}

void KeyedStoreGenericGenerator::SetProperty(
    compiler::CodeAssemblerState* state, TNode<Context> context,
    TNode<JSReceiver> receiver, TNode<BoolT> is_simple_receiver,
    TNode<Name> name, TNode<Object> value, LanguageMode language_mode) {
  KeyedStoreGenericAssembler assembler(state, StoreMode::kOrdinary);
  assembler.SetProperty(context, receiver, is_simple_receiver, name, value,
                        language_mode);
}

void KeyedStoreGenericGenerator::SetProperty(
    compiler::CodeAssemblerState* state, TNode<Context> context,
    TNode<Object> receiver, TNode<Object> key, TNode<Object> value,
    LanguageMode language_mode) {
  KeyedStoreGenericAssembler assembler(state, StoreMode::kOrdinary);
  assembler.SetProperty(context, receiver, key, value, language_mode);
}

void KeyedStoreGenericGenerator::SetPropertyInLiteral(
    compiler::CodeAssemblerState* state, TNode<Context> context,
    TNode<JSObject> receiver, TNode<Object> key, TNode<Object> value) {
  KeyedStoreGenericAssembler assembler(state, StoreMode::kInLiteral);
  assembler.SetProperty(context, receiver, key, value, LanguageMode::kStrict);
}

void KeyedStoreGenericAssembler::BranchIfPrototypesHaveNonFastElements(
    Node* receiver_map, Label* non_fast_elements, Label* only_fast_elements) {
  VARIABLE(var_map, MachineRepresentation::kTagged);
  var_map.Bind(receiver_map);
  Label loop_body(this, &var_map);
  Goto(&loop_body);

  BIND(&loop_body);
  {
    Node* map = var_map.value();
    Node* prototype = LoadMapPrototype(map);
    GotoIf(IsNull(prototype), only_fast_elements);
    Node* prototype_map = LoadMap(prototype);
    var_map.Bind(prototype_map);
    TNode<Int32T> instance_type = LoadMapInstanceType(prototype_map);
    GotoIf(IsCustomElementsReceiverInstanceType(instance_type),
           non_fast_elements);
    TNode<Int32T> elements_kind = LoadMapElementsKind(prototype_map);
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
  if (AllocationSite::ShouldTrack(from_kind, to_kind)) {
    TrapAllocationMemento(receiver, bailout);
  }
  Label perform_transition(this), check_holey_map(this);
  VARIABLE(var_target_map, MachineRepresentation::kTagged);
  // Check if the receiver has the default |from_kind| map.
  {
    Node* packed_map = LoadJSArrayElementsMap(from_kind, native_context);
    GotoIf(WordNotEqual(receiver_map, packed_map), &check_holey_map);
    var_target_map.Bind(
        LoadContextElement(native_context, Context::ArrayMapIndex(to_kind)));
    Goto(&perform_transition);
  }

  // Check if the receiver has the default |holey_from_kind| map.
  BIND(&check_holey_map);
  {
    Node* holey_map = LoadContextElement(
        native_context, Context::ArrayMapIndex(holey_from_kind));
    GotoIf(WordNotEqual(receiver_map, holey_map), bailout);
    var_target_map.Bind(LoadContextElement(
        native_context, Context::ArrayMapIndex(holey_to_kind)));
    Goto(&perform_transition);
  }

  // Found a supported transition target map, perform the transition!
  BIND(&perform_transition);
  {
    if (IsDoubleElementsKind(from_kind) != IsDoubleElementsKind(to_kind)) {
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
  Node* packed_map = LoadJSArrayElementsMap(packed_kind, native_context);
  GotoIf(WordNotEqual(receiver_map, packed_map), map_mismatch);
  if (AllocationSite::ShouldTrack(packed_kind, holey_kind)) {
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
  BIND(&already_holey);
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
  BIND(&check_other_kind);
  TryChangeToHoleyMapHelper(receiver, receiver_map, native_context,
                            packed_kind_2, holey_kind_2, &already_holey,
                            bailout, bailout);
  BIND(&already_holey);
}

void KeyedStoreGenericAssembler::MaybeUpdateLengthAndReturn(
    Node* receiver, Node* index, Node* value, UpdateLength update_length) {
  if (update_length != kDontChangeLength) {
    Node* new_length = SmiTag(Signed(IntPtrAdd(index, IntPtrConstant(1))));
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
    CSA_ASSERT(this, InstanceTypeEqual(LoadMapInstanceType(receiver_map),
                                       JS_ARRAY_TYPE));
    // Check if the length property is writable. The fast check is only
    // supported for fast properties.
    GotoIf(IsDictionaryMap(receiver_map), slow);
    // The length property is non-configurable, so it's guaranteed to always
    // be the first property.
    TNode<DescriptorArray> descriptors = LoadMapDescriptors(receiver_map);
    TNode<Uint32T> details = LoadDetailsByDescriptorEntry(descriptors, 0);
    GotoIf(IsSetWord32(details, PropertyDetails::kAttributesReadOnlyMask),
           slow);
  }
  STATIC_ASSERT(FixedArray::kHeaderSize == FixedDoubleArray::kHeaderSize);
  const int kHeaderSize = FixedArray::kHeaderSize - kHeapObjectTag;

  Label check_double_elements(this), check_cow_elements(this);
  Node* elements_map = LoadMap(elements);
  GotoIf(WordNotEqual(elements_map, LoadRoot(RootIndex::kFixedArrayMap)),
         &check_double_elements);

  // FixedArray backing store -> Smi or object elements.
  {
    Node* offset = ElementOffsetFromIndex(intptr_index, PACKED_ELEMENTS,
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
      BIND(&hole_check_passed);
    }

    // Check if the value we're storing matches the elements_kind. Smis
    // can always be stored.
    {
      Label non_smi_value(this);
      GotoIfNot(TaggedIsSmi(value), &non_smi_value);
      // If we're about to introduce holes, ensure holey elements.
      if (update_length == kBumpLengthWithGap) {
        TryChangeToHoleyMapMulti(receiver, receiver_map, elements_kind, context,
                                 PACKED_SMI_ELEMENTS, PACKED_ELEMENTS, slow);
      }
      StoreNoWriteBarrier(MachineRepresentation::kTaggedSigned, elements,
                          offset, value);
      MaybeUpdateLengthAndReturn(receiver, intptr_index, value, update_length);

      BIND(&non_smi_value);
    }

    // Check if we already have object elements; just do the store if so.
    {
      Label must_transition(this);
      STATIC_ASSERT(PACKED_SMI_ELEMENTS == 0);
      STATIC_ASSERT(HOLEY_SMI_ELEMENTS == 1);
      GotoIf(Int32LessThanOrEqual(elements_kind,
                                  Int32Constant(HOLEY_SMI_ELEMENTS)),
             &must_transition);
      if (update_length == kBumpLengthWithGap) {
        TryChangeToHoleyMap(receiver, receiver_map, elements_kind, context,
                            PACKED_ELEMENTS, slow);
      }
      Store(elements, offset, value);
      MaybeUpdateLengthAndReturn(receiver, intptr_index, value, update_length);

      BIND(&must_transition);
    }

    // Transition to the required ElementsKind.
    {
      Label transition_to_double(this), transition_to_object(this);
      Node* native_context = LoadNativeContext(context);
      Branch(WordEqual(LoadMap(value), LoadRoot(RootIndex::kHeapNumberMap)),
             &transition_to_double, &transition_to_object);
      BIND(&transition_to_double);
      {
        // If we're adding holes at the end, always transition to a holey
        // elements kind, otherwise try to remain packed.
        ElementsKind target_kind = update_length == kBumpLengthWithGap
                                       ? HOLEY_DOUBLE_ELEMENTS
                                       : PACKED_DOUBLE_ELEMENTS;
        TryRewriteElements(receiver, receiver_map, elements, native_context,
                           PACKED_SMI_ELEMENTS, target_kind, slow);
        // Reload migrated elements.
        Node* double_elements = LoadElements(receiver);
        Node* double_offset =
            ElementOffsetFromIndex(intptr_index, PACKED_DOUBLE_ELEMENTS,
                                   INTPTR_PARAMETERS, kHeaderSize);
        // Make sure we do not store signalling NaNs into double arrays.
        Node* double_value = Float64SilenceNaN(LoadHeapNumberValue(value));
        StoreNoWriteBarrier(MachineRepresentation::kFloat64, double_elements,
                            double_offset, double_value);
        MaybeUpdateLengthAndReturn(receiver, intptr_index, value,
                                   update_length);
      }

      BIND(&transition_to_object);
      {
        // If we're adding holes at the end, always transition to a holey
        // elements kind, otherwise try to remain packed.
        ElementsKind target_kind = update_length == kBumpLengthWithGap
                                       ? HOLEY_ELEMENTS
                                       : PACKED_ELEMENTS;
        TryRewriteElements(receiver, receiver_map, elements, native_context,
                           PACKED_SMI_ELEMENTS, target_kind, slow);
        // The elements backing store didn't change, no reload necessary.
        CSA_ASSERT(this, WordEqual(elements, LoadElements(receiver)));
        Store(elements, offset, value);
        MaybeUpdateLengthAndReturn(receiver, intptr_index, value,
                                   update_length);
      }
    }
  }

  BIND(&check_double_elements);
  Node* fixed_double_array_map = LoadRoot(RootIndex::kFixedDoubleArrayMap);
  GotoIf(WordNotEqual(elements_map, fixed_double_array_map),
         &check_cow_elements);
  // FixedDoubleArray backing store -> double elements.
  {
    Node* offset = ElementOffsetFromIndex(intptr_index, PACKED_DOUBLE_ELEMENTS,
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
        BIND(&found_hole);
      }
      BranchIfPrototypesHaveNonFastElements(receiver_map, slow,
                                            &hole_check_passed);
      BIND(&hole_check_passed);
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
                            PACKED_DOUBLE_ELEMENTS, slow);
      }
      StoreNoWriteBarrier(MachineRepresentation::kFloat64, elements, offset,
                          double_value);
      MaybeUpdateLengthAndReturn(receiver, intptr_index, value, update_length);

      BIND(&non_number_value);
    }

    // Transition to object elements.
    {
      Node* native_context = LoadNativeContext(context);
      ElementsKind target_kind = update_length == kBumpLengthWithGap
                                     ? HOLEY_ELEMENTS
                                     : PACKED_ELEMENTS;
      TryRewriteElements(receiver, receiver_map, elements, native_context,
                         PACKED_DOUBLE_ELEMENTS, target_kind, slow);
      // Reload migrated elements.
      Node* fast_elements = LoadElements(receiver);
      Node* fast_offset = ElementOffsetFromIndex(
          intptr_index, PACKED_ELEMENTS, INTPTR_PARAMETERS, kHeaderSize);
      Store(fast_elements, fast_offset, value);
      MaybeUpdateLengthAndReturn(receiver, intptr_index, value, update_length);
    }
  }

  BIND(&check_cow_elements);
  {
    // TODO(jkummerow): Use GrowElementsCapacity instead of bailing out.
    Goto(slow);
  }
}

void KeyedStoreGenericAssembler::EmitGenericElementStore(
    Node* receiver, Node* receiver_map, Node* instance_type, Node* intptr_index,
    Node* value, Node* context, Label* slow) {
  Label if_fast(this), if_in_bounds(this), if_out_of_bounds(this),
      if_increment_length_by_one(this), if_bump_length_with_gap(this),
      if_grow(this), if_nonfast(this), if_typed_array(this),
      if_dictionary(this);
  Node* elements = LoadElements(receiver);
  TNode<Int32T> elements_kind = LoadMapElementsKind(receiver_map);
  Branch(IsFastElementsKind(elements_kind), &if_fast, &if_nonfast);
  BIND(&if_fast);

  Label if_array(this);
  GotoIf(InstanceTypeEqual(instance_type, JS_ARRAY_TYPE), &if_array);
  {
    Node* capacity = SmiUntag(LoadFixedArrayBaseLength(elements));
    Branch(UintPtrLessThan(intptr_index, capacity), &if_in_bounds,
           &if_out_of_bounds);
  }
  BIND(&if_array);
  {
    Node* length = SmiUntag(LoadFastJSArrayLength(receiver));
    GotoIf(UintPtrLessThan(intptr_index, length), &if_in_bounds);
    Node* capacity = SmiUntag(LoadFixedArrayBaseLength(elements));
    GotoIf(UintPtrGreaterThanOrEqual(intptr_index, capacity), &if_grow);
    Branch(WordEqual(intptr_index, length), &if_increment_length_by_one,
           &if_bump_length_with_gap);
  }

  BIND(&if_in_bounds);
  {
    StoreElementWithCapacity(receiver, receiver_map, elements, elements_kind,
                             intptr_index, value, context, slow,
                             kDontChangeLength);
  }

  BIND(&if_out_of_bounds);
  {
    // Integer indexed out-of-bounds accesses to typed arrays are simply
    // ignored, since we never look up integer indexed properties on the
    // prototypes of typed arrays. For all other types, we may need to
    // grow the backing store.
    GotoIfNot(InstanceTypeEqual(instance_type, JS_TYPED_ARRAY_TYPE), &if_grow);
    Return(value);
  }

  BIND(&if_increment_length_by_one);
  {
    StoreElementWithCapacity(receiver, receiver_map, elements, elements_kind,
                             intptr_index, value, context, slow,
                             kIncrementLengthByOne);
  }

  BIND(&if_bump_length_with_gap);
  {
    StoreElementWithCapacity(receiver, receiver_map, elements, elements_kind,
                             intptr_index, value, context, slow,
                             kBumpLengthWithGap);
  }

  // Out-of-capacity accesses (index >= capacity) jump here. Additionally,
  // an ElementsKind transition might be necessary.
  // The index can also be negative at this point! Jump to the runtime in that
  // case to convert it to a named property.
  BIND(&if_grow);
  {
    Comment("Grow backing store");
    // TODO(jkummerow): Support inline backing store growth.
    Goto(slow);
  }

  // Any ElementsKind > LAST_FAST_ELEMENTS_KIND jumps here for further
  // dispatch.
  BIND(&if_nonfast);
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

  BIND(&if_dictionary);
  {
    Comment("Dictionary");
    // TODO(jkummerow): Support storing to dictionary elements.
    Goto(slow);
  }

  BIND(&if_typed_array);
  {
    Comment("Typed array");
    // TODO(jkummerow): Support typed arrays.
    Goto(slow);
  }
}

void KeyedStoreGenericAssembler::LookupPropertyOnPrototypeChain(
    Node* receiver_map, Node* name, Label* accessor,
    Variable* var_accessor_pair, Variable* var_accessor_holder, Label* readonly,
    Label* bailout) {
  Label ok_to_write(this);
  VARIABLE(var_holder, MachineRepresentation::kTagged);
  var_holder.Bind(LoadMapPrototype(receiver_map));
  VARIABLE(var_holder_map, MachineRepresentation::kTagged);
  var_holder_map.Bind(LoadMap(var_holder.value()));

  Variable* merged_variables[] = {&var_holder, &var_holder_map};
  Label loop(this, arraysize(merged_variables), merged_variables);
  Goto(&loop);
  BIND(&loop);
  {
    Node* holder = var_holder.value();
    GotoIf(IsNull(holder), &ok_to_write);
    Node* holder_map = var_holder_map.value();
    Node* instance_type = LoadMapInstanceType(holder_map);
    Label next_proto(this);
    {
      Label found(this), found_fast(this), found_dict(this), found_global(this);
      TVARIABLE(HeapObject, var_meta_storage);
      TVARIABLE(IntPtrT, var_entry);
      TryLookupProperty(holder, holder_map, instance_type, name, &found_fast,
                        &found_dict, &found_global, &var_meta_storage,
                        &var_entry, &next_proto, bailout);
      BIND(&found_fast);
      {
        TNode<DescriptorArray> descriptors = CAST(var_meta_storage.value());
        TNode<IntPtrT> name_index = var_entry.value();
        Node* details = LoadDetailsByKeyIndex(descriptors, name_index);
        JumpIfDataProperty(details, &ok_to_write, readonly);

        // Accessor case.
        // TODO(jkummerow): Implement a trimmed-down
        // LoadAccessorFromFastObject.
        VARIABLE(var_details, MachineRepresentation::kWord32);
        LoadPropertyFromFastObject(holder, holder_map, descriptors, name_index,
                                   &var_details, var_accessor_pair);
        var_accessor_holder->Bind(holder);
        Goto(accessor);
      }

      BIND(&found_dict);
      {
        Node* dictionary = var_meta_storage.value();
        Node* entry = var_entry.value();
        Node* details =
            LoadDetailsByKeyIndex<NameDictionary>(dictionary, entry);
        JumpIfDataProperty(details, &ok_to_write, readonly);

        if (accessor != nullptr) {
          // Accessor case.
          var_accessor_pair->Bind(
              LoadValueByKeyIndex<NameDictionary>(dictionary, entry));
          var_accessor_holder->Bind(holder);
          Goto(accessor);
        } else {
          Goto(&ok_to_write);
        }
      }

      BIND(&found_global);
      {
        Node* dictionary = var_meta_storage.value();
        Node* entry = var_entry.value();
        Node* property_cell =
            LoadValueByKeyIndex<GlobalDictionary>(dictionary, entry);
        Node* value =
            LoadObjectField(property_cell, PropertyCell::kValueOffset);
        GotoIf(WordEqual(value, TheHoleConstant()), &next_proto);
        Node* details = LoadAndUntagToWord32ObjectField(
            property_cell, PropertyCell::kPropertyDetailsRawOffset);
        JumpIfDataProperty(details, &ok_to_write, readonly);

        if (accessor != nullptr) {
          // Accessor case.
          var_accessor_pair->Bind(value);
          var_accessor_holder->Bind(holder);
          Goto(accessor);
        } else {
          Goto(&ok_to_write);
        }
      }
    }

    BIND(&next_proto);
    // Bailout if it can be an integer indexed exotic case.
    GotoIf(InstanceTypeEqual(instance_type, JS_TYPED_ARRAY_TYPE), bailout);
    Node* proto = LoadMapPrototype(holder_map);
    GotoIf(IsNull(proto), &ok_to_write);
    var_holder.Bind(proto);
    var_holder_map.Bind(LoadMap(proto));
    Goto(&loop);
  }
  BIND(&ok_to_write);
}

TNode<Map> KeyedStoreGenericAssembler::FindCandidateStoreICTransitionMapHandler(
    TNode<Map> map, TNode<Name> name, Label* slow) {
  TVARIABLE(Map, var_transition_map);
  Label simple_transition(this), transition_array(this),
      found_handler_candidate(this);

  TNode<MaybeObject> maybe_handler =
      LoadMaybeWeakObjectField(map, Map::kTransitionsOrPrototypeInfoOffset);

  // Smi -> slow,
  // Cleared weak reference -> slow
  // weak reference -> simple_transition
  // strong reference -> transition_array
  TVARIABLE(Object, var_transition_map_or_array);
  DispatchMaybeObject(maybe_handler, slow, slow, &simple_transition,
                      &transition_array, &var_transition_map_or_array);

  BIND(&simple_transition);
  {
    var_transition_map = CAST(var_transition_map_or_array.value());
    Goto(&found_handler_candidate);
  }

  BIND(&transition_array);
  {
    TNode<Map> maybe_handler_map =
        LoadMap(CAST(var_transition_map_or_array.value()));
    GotoIfNot(IsTransitionArrayMap(maybe_handler_map), slow);

    TVARIABLE(IntPtrT, var_name_index);
    Label if_found_candidate(this);
    TNode<TransitionArray> transitions =
        CAST(var_transition_map_or_array.value());
    TransitionLookup(name, transitions, &if_found_candidate, &var_name_index,
                     slow);

    BIND(&if_found_candidate);
    {
      // Given that
      // 1) transitions with the same name are ordered in the transition
      //    array by PropertyKind and then by PropertyAttributes values,
      // 2) kData < kAccessor,
      // 3) NONE == 0,
      // 4) properties with private symbol names are guaranteed to be
      //    non-enumerable (so DONT_ENUM bit in attributes is always set),
      // the resulting map of transitioning store if it exists in the
      // transition array is expected to be the first among the transitions
      // with the same name.
      // See TransitionArray::CompareDetails() for details.
      STATIC_ASSERT(kData == 0);
      STATIC_ASSERT(NONE == 0);
      const int kKeyToTargetOffset = (TransitionArray::kEntryTargetIndex -
                                      TransitionArray::kEntryKeyIndex) *
                                     kTaggedSize;
      var_transition_map = CAST(GetHeapObjectAssumeWeak(
          LoadArrayElement(transitions, WeakFixedArray::kHeaderSize,
                           var_name_index.value(), kKeyToTargetOffset)));
      Goto(&found_handler_candidate);
    }
  }

  BIND(&found_handler_candidate);
  return var_transition_map.value();
}

void KeyedStoreGenericAssembler::EmitGenericPropertyStore(
    TNode<JSReceiver> receiver, TNode<Map> receiver_map,
    const StoreICParameters* p, ExitPoint* exit_point, Label* slow,
    Maybe<LanguageMode> maybe_language_mode) {
  CSA_ASSERT(this, IsSimpleObjectMap(receiver_map));
  VARIABLE(var_accessor_pair, MachineRepresentation::kTagged);
  VARIABLE(var_accessor_holder, MachineRepresentation::kTagged);
  Label fast_properties(this), dictionary_properties(this), accessor(this),
      readonly(this);
  Node* bitfield3 = LoadMapBitField3(receiver_map);
  Branch(IsSetWord32<Map::IsDictionaryMapBit>(bitfield3),
         &dictionary_properties, &fast_properties);

  BIND(&fast_properties);
  {
    Comment("fast property store");
    TNode<DescriptorArray> descriptors = LoadMapDescriptors(receiver_map);
    Label descriptor_found(this), lookup_transition(this);
    TVARIABLE(IntPtrT, var_name_index);
    DescriptorLookup(p->name(), descriptors, bitfield3, &descriptor_found,
                     &var_name_index, &lookup_transition);

    BIND(&descriptor_found);
    {
      TNode<IntPtrT> name_index = var_name_index.value();
      Node* details = LoadDetailsByKeyIndex(descriptors, name_index);
      Label data_property(this);
      JumpIfDataProperty(details, &data_property,
                         ShouldReconfigureExisting() ? nullptr : &readonly);

      if (ShouldCallSetter()) {
        // Accessor case.
        // TODO(jkummerow): Implement a trimmed-down LoadAccessorFromFastObject.
        VARIABLE(var_details, MachineRepresentation::kWord32);
        LoadPropertyFromFastObject(receiver, receiver_map, descriptors,
                                   name_index, &var_details,
                                   &var_accessor_pair);
        var_accessor_holder.Bind(receiver);
        Goto(&accessor);
      } else {
        Goto(&data_property);
      }

      BIND(&data_property);
      {
        CheckForAssociatedProtector(p->name(), slow);
        OverwriteExistingFastDataProperty(receiver, receiver_map, descriptors,
                                          name_index, details, p->value(), slow,
                                          false);
        exit_point->Return(p->value());
      }
    }
    BIND(&lookup_transition);
    {
      Comment("lookup transition");
      TNode<Map> transition_map = FindCandidateStoreICTransitionMapHandler(
          receiver_map, CAST(p->name()), slow);

      // Validate the transition handler candidate and apply the transition.
      StoreTransitionMapFlags flags = kValidateTransitionHandler;
      if (ShouldCheckPrototypeValidity()) {
        flags = StoreTransitionMapFlags(flags | kCheckPrototypeValidity);
      }
      HandleStoreICTransitionMapHandlerCase(p, transition_map, slow, flags);
      exit_point->Return(p->value());
    }
  }

  BIND(&dictionary_properties);
  {
    Comment("dictionary property store");
    // We checked for LAST_CUSTOM_ELEMENTS_RECEIVER before, which rules out
    // seeing global objects here (which would need special handling).

    TVARIABLE(IntPtrT, var_name_index);
    Label dictionary_found(this, &var_name_index), not_found(this);
    TNode<NameDictionary> properties = CAST(LoadSlowProperties(CAST(receiver)));
    NameDictionaryLookup<NameDictionary>(properties, CAST(p->name()),
                                         &dictionary_found, &var_name_index,
                                         &not_found);
    BIND(&dictionary_found);
    {
      Label overwrite(this);
      TNode<Uint32T> details = LoadDetailsByKeyIndex<NameDictionary>(
          properties, var_name_index.value());
      JumpIfDataProperty(details, &overwrite,
                         ShouldReconfigureExisting() ? nullptr : &readonly);

      if (ShouldCallSetter()) {
        // Accessor case.
        var_accessor_pair.Bind(LoadValueByKeyIndex<NameDictionary>(
            properties, var_name_index.value()));
        var_accessor_holder.Bind(receiver);
        Goto(&accessor);
      } else {
        // We must reconfigure an accessor property to a data property
        // here, let the runtime take care of that.
        Goto(slow);
      }

      BIND(&overwrite);
      {
        CheckForAssociatedProtector(p->name(), slow);
        StoreValueByKeyIndex<NameDictionary>(properties, var_name_index.value(),
                                             p->value());
        exit_point->Return(p->value());
      }
    }

    BIND(&not_found);
    {
      CheckForAssociatedProtector(p->name(), slow);
      Label extensible(this), is_private_symbol(this);
      Node* bitfield3 = LoadMapBitField3(receiver_map);
      GotoIf(IsPrivateSymbol(p->name()), &is_private_symbol);
      Branch(IsSetWord32<Map::IsExtensibleBit>(bitfield3), &extensible, slow);

      BIND(&is_private_symbol);
      {
        CSA_ASSERT(this, IsPrivateSymbol(p->name()));
        // For private names, we miss to the runtime which will throw.
        // For private symbols, we extend and store an own property.
        Branch(IsPrivateName(p->name()), slow, &extensible);
      }

      BIND(&extensible);
      if (ShouldCheckPrototype()) {
        DCHECK(ShouldCallSetter());
        LookupPropertyOnPrototypeChain(
            receiver_map, p->name(), &accessor, &var_accessor_pair,
            &var_accessor_holder,
            ShouldReconfigureExisting() ? nullptr : &readonly, slow);
      }
      Label add_dictionary_property_slow(this);
      InvalidateValidityCellIfPrototype(receiver_map, bitfield3);
      Add<NameDictionary>(properties, CAST(p->name()), p->value(),
                          &add_dictionary_property_slow);
      exit_point->Return(p->value());

      BIND(&add_dictionary_property_slow);
      exit_point->ReturnCallRuntime(Runtime::kAddDictionaryProperty,
                                    p->context(), p->receiver(), p->name(),
                                    p->value());
    }
  }

  if (ShouldCallSetter()) {
    BIND(&accessor);
    {
      Label not_callable(this);
      Node* accessor_pair = var_accessor_pair.value();
      GotoIf(IsAccessorInfoMap(LoadMap(accessor_pair)), slow);
      CSA_ASSERT(this, HasInstanceType(accessor_pair, ACCESSOR_PAIR_TYPE));
      Node* setter =
          LoadObjectField(accessor_pair, AccessorPair::kSetterOffset);
      Node* setter_map = LoadMap(setter);
      // FunctionTemplateInfo setters are not supported yet.
      GotoIf(IsFunctionTemplateInfoMap(setter_map), slow);
      GotoIfNot(IsCallableMap(setter_map), &not_callable);

      Callable callable = CodeFactory::Call(isolate());
      CallJS(callable, p->context(), setter, receiver, p->value());
      exit_point->Return(p->value());

      BIND(&not_callable);
      {
        LanguageMode language_mode;
        if (maybe_language_mode.To(&language_mode)) {
          if (language_mode == LanguageMode::kStrict) {
            exit_point->ReturnCallRuntime(
                Runtime::kThrowTypeError, p->context(),
                SmiConstant(MessageTemplate::kNoSetterInCallback), p->name(),
                var_accessor_holder.value());
          } else {
            exit_point->Return(p->value());
          }
        } else {
          CallRuntime(Runtime::kThrowTypeErrorIfStrict, p->context(),
                      SmiConstant(MessageTemplate::kNoSetterInCallback),
                      p->name(), var_accessor_holder.value());
          exit_point->Return(p->value());
        }
      }
    }
  }

  if (!ShouldReconfigureExisting()) {
    BIND(&readonly);
    {
      LanguageMode language_mode;
      if (maybe_language_mode.To(&language_mode)) {
        if (language_mode == LanguageMode::kStrict) {
          Node* type = Typeof(p->receiver());
          ThrowTypeError(p->context(), MessageTemplate::kStrictReadOnlyProperty,
                         p->name(), type, p->receiver());
        } else {
          exit_point->Return(p->value());
        }
      } else {
        CallRuntime(Runtime::kThrowTypeErrorIfStrict, p->context(),
                    SmiConstant(MessageTemplate::kStrictReadOnlyProperty),
                    p->name(), Typeof(p->receiver()), p->receiver());
        exit_point->Return(p->value());
      }
    }
  }
}

// Helper that is used by the public KeyedStoreGeneric and by SetProperty.
void KeyedStoreGenericAssembler::KeyedStoreGeneric(
    TNode<Context> context, TNode<Object> receiver, TNode<Object> key,
    TNode<Object> value, Maybe<LanguageMode> language_mode) {
  TVARIABLE(IntPtrT, var_index);
  TVARIABLE(Object, var_unique, key);
  Label if_index(this), if_unique_name(this), not_internalized(this),
      slow(this);

  GotoIf(TaggedIsSmi(receiver), &slow);
  TNode<Map> receiver_map = LoadMap(CAST(receiver));
  TNode<Int32T> instance_type = LoadMapInstanceType(receiver_map);
  // Receivers requiring non-standard element accesses (interceptors, access
  // checks, strings and string wrappers, proxies) are handled in the runtime.
  GotoIf(IsCustomElementsReceiverInstanceType(instance_type), &slow);

  TryToName(key, &if_index, &var_index, &if_unique_name, &var_unique, &slow,
            &not_internalized);

  BIND(&if_index);
  {
    Comment("integer index");
    EmitGenericElementStore(receiver, receiver_map, instance_type,
                            var_index.value(), value, context, &slow);
  }

  BIND(&if_unique_name);
  {
    Comment("key is unique name");
    StoreICParameters p(context, receiver, var_unique.value(), value, nullptr,
                        nullptr);
    ExitPoint direct_exit(this);
    EmitGenericPropertyStore(CAST(receiver), receiver_map, &p, &direct_exit,
                             &slow, language_mode);
  }

  BIND(&not_internalized);
  {
    if (FLAG_internalize_on_the_fly) {
      TryInternalizeString(key, &if_index, &var_index, &if_unique_name,
                           &var_unique, &slow, &slow);
    } else {
      Goto(&slow);
    }
  }

  BIND(&slow);
  {
    if (IsKeyedStore()) {
      Comment("KeyedStoreGeneric_slow");
      TailCallRuntime(Runtime::kSetKeyedProperty, context, receiver, key,
                      value);
    } else {
      DCHECK(IsStoreInLiteral());
      TailCallRuntime(Runtime::kStoreDataPropertyInLiteral, context, receiver,
                      key, value);
    }
  }
}

void KeyedStoreGenericAssembler::KeyedStoreGeneric() {
  using Descriptor = StoreDescriptor;

  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> name = CAST(Parameter(Descriptor::kName));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  KeyedStoreGeneric(context, receiver, name, value, Nothing<LanguageMode>());
}

void KeyedStoreGenericAssembler::SetProperty(TNode<Context> context,
                                             TNode<Object> receiver,
                                             TNode<Object> key,
                                             TNode<Object> value,
                                             LanguageMode language_mode) {
  KeyedStoreGeneric(context, receiver, key, value, Just(language_mode));
}

void KeyedStoreGenericAssembler::StoreIC_Uninitialized() {
  using Descriptor = StoreWithVectorDescriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label miss(this, Label::kDeferred), store_property(this);

  GotoIf(TaggedIsSmi(receiver), &miss);
  Node* receiver_map = LoadMap(receiver);
  TNode<Int32T> instance_type = LoadMapInstanceType(receiver_map);
  // Receivers requiring non-standard element accesses (interceptors, access
  // checks, strings and string wrappers, proxies) are handled in the runtime.
  GotoIf(IsSpecialReceiverInstanceType(instance_type), &miss);

  // Optimistically write the state transition to the vector.
  GotoIf(IsUndefined(vector), &store_property);
  StoreFeedbackVectorSlot(vector, slot,
                          LoadRoot(RootIndex::kpremonomorphic_symbol),
                          SKIP_WRITE_BARRIER, 0, SMI_PARAMETERS);
  Goto(&store_property);

  BIND(&store_property);
  {
    StoreICParameters p(CAST(context), receiver, name, value, slot, vector);
    EmitGenericPropertyStore(receiver, receiver_map, &p, &miss);
  }

  BIND(&miss);
  {
    Label call_runtime(this);
    // Undo the optimistic state transition.
    GotoIf(IsUndefined(vector), &call_runtime);
    StoreFeedbackVectorSlot(vector, slot,
                            LoadRoot(RootIndex::kuninitialized_symbol),
                            SKIP_WRITE_BARRIER, 0, SMI_PARAMETERS);
    Goto(&call_runtime);

    BIND(&call_runtime);
    TailCallRuntime(Runtime::kStoreIC_Miss, context, value, slot, vector,
                    receiver, name);
  }
}

void KeyedStoreGenericAssembler::SetProperty(TNode<Context> context,
                                             TNode<JSReceiver> receiver,
                                             TNode<BoolT> is_simple_receiver,
                                             TNode<Name> unique_name,
                                             TNode<Object> value,
                                             LanguageMode language_mode) {
  StoreICParameters p(context, receiver, unique_name, value, nullptr, nullptr);

  Label done(this), slow(this, Label::kDeferred);
  ExitPoint exit_point(this, [&](Node* result) { Goto(&done); });

  CSA_ASSERT(this, Word32Equal(is_simple_receiver,
                               IsSimpleObjectMap(LoadMap(receiver))));
  GotoIfNot(is_simple_receiver, &slow);

  EmitGenericPropertyStore(receiver, LoadMap(receiver), &p, &exit_point, &slow,
                           Just(language_mode));

  BIND(&slow);
  {
    if (IsStoreInLiteral()) {
      CallRuntime(Runtime::kStoreDataPropertyInLiteral, context, receiver,
                  unique_name, value);
    } else {
      CallRuntime(Runtime::kSetKeyedProperty, context, receiver, unique_name,
                  value);
    }
    Goto(&done);
  }

  BIND(&done);
}

}  // namespace internal
}  // namespace v8
