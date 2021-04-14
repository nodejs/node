// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/map-updater.h"

#include <queue>

#include "src/execution/frames.h"
#include "src/execution/isolate.h"
#include "src/handles/handles.h"
#include "src/objects/field-type.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/objects/property-details.h"
#include "src/objects/transitions.h"

namespace v8 {
namespace internal {

namespace {

inline bool EqualImmutableValues(Object obj1, Object obj2) {
  if (obj1 == obj2) return true;  // Valid for both kData and kAccessor kinds.
  // TODO(ishell): compare AccessorPairs.
  return false;
}

V8_WARN_UNUSED_RESULT Handle<FieldType> GeneralizeFieldType(
    Representation rep1, Handle<FieldType> type1, Representation rep2,
    Handle<FieldType> type2, Isolate* isolate) {
  // Cleared field types need special treatment. They represent lost knowledge,
  // so we must be conservative, so their generalization with any other type
  // is "Any".
  if (Map::FieldTypeIsCleared(rep1, *type1) ||
      Map::FieldTypeIsCleared(rep2, *type2)) {
    return FieldType::Any(isolate);
  }
  if (type1->NowIs(type2)) return type2;
  if (type2->NowIs(type1)) return type1;
  return FieldType::Any(isolate);
}

void PrintGeneralization(
    Isolate* isolate, Handle<Map> map, FILE* file, const char* reason,
    InternalIndex modify_index, int split, int descriptors,
    bool descriptor_to_field, Representation old_representation,
    Representation new_representation, PropertyConstness old_constness,
    PropertyConstness new_constness, MaybeHandle<FieldType> old_field_type,
    MaybeHandle<Object> old_value, MaybeHandle<FieldType> new_field_type,
    MaybeHandle<Object> new_value) {
  OFStream os(file);
  os << "[generalizing]";
  Name name = map->instance_descriptors(isolate).GetKey(modify_index);
  if (name.IsString()) {
    String::cast(name).PrintOn(file);
  } else {
    os << "{symbol " << reinterpret_cast<void*>(name.ptr()) << "}";
  }
  os << ":";
  if (descriptor_to_field) {
    os << "c";
  } else {
    os << old_representation.Mnemonic() << "{";
    if (old_field_type.is_null()) {
      os << Brief(*(old_value.ToHandleChecked()));
    } else {
      old_field_type.ToHandleChecked()->PrintTo(os);
    }
    os << ";" << old_constness << "}";
  }
  os << "->" << new_representation.Mnemonic() << "{";
  if (new_field_type.is_null()) {
    os << Brief(*(new_value.ToHandleChecked()));
  } else {
    new_field_type.ToHandleChecked()->PrintTo(os);
  }
  os << ";" << new_constness << "} (";
  if (strlen(reason) > 0) {
    os << reason;
  } else {
    os << "+" << (descriptors - split) << " maps";
  }
  os << ") [";
  JavaScriptFrame::PrintTop(isolate, file, false, true);
  os << "]\n";
}

}  // namespace

MapUpdater::MapUpdater(Isolate* isolate, Handle<Map> old_map)
    : isolate_(isolate),
      old_map_(old_map),
      old_descriptors_(old_map->instance_descriptors(isolate), isolate_),
      old_nof_(old_map_->NumberOfOwnDescriptors()),
      new_elements_kind_(old_map_->elements_kind()),
      is_transitionable_fast_elements_kind_(
          IsTransitionableFastElementsKind(new_elements_kind_)) {
  // We shouldn't try to update remote objects.
  DCHECK(
      !old_map->FindRootMap(isolate).GetConstructor().IsFunctionTemplateInfo());
}

Name MapUpdater::GetKey(InternalIndex descriptor) const {
  return old_descriptors_->GetKey(descriptor);
}

PropertyDetails MapUpdater::GetDetails(InternalIndex descriptor) const {
  DCHECK(descriptor.is_found());
  if (descriptor == modified_descriptor_) {
    PropertyAttributes attributes = new_attributes_;
    // If the original map was sealed or frozen, let's use the old
    // attributes so that we follow the same transition path as before.
    // Note that the user could not have changed the attributes because
    // both seal and freeze make the properties non-configurable. An exception
    // is transitioning from [[Writable]] = true to [[Writable]] = false (this
    // is allowed for frozen and sealed objects). To support it, we use the new
    // attributes if they have [[Writable]] == false.
    if ((integrity_level_ == SEALED || integrity_level_ == FROZEN) &&
        !(new_attributes_ & READ_ONLY)) {
      attributes = old_descriptors_->GetDetails(descriptor).attributes();
    }
    return PropertyDetails(new_kind_, attributes, new_location_, new_constness_,
                           new_representation_);
  }
  return old_descriptors_->GetDetails(descriptor);
}

Object MapUpdater::GetValue(InternalIndex descriptor) const {
  DCHECK(descriptor.is_found());
  if (descriptor == modified_descriptor_) {
    DCHECK_EQ(kDescriptor, new_location_);
    return *new_value_;
  }
  DCHECK_EQ(kDescriptor, GetDetails(descriptor).location());
  return old_descriptors_->GetStrongValue(descriptor);
}

FieldType MapUpdater::GetFieldType(InternalIndex descriptor) const {
  DCHECK(descriptor.is_found());
  if (descriptor == modified_descriptor_) {
    DCHECK_EQ(kField, new_location_);
    return *new_field_type_;
  }
  DCHECK_EQ(kField, GetDetails(descriptor).location());
  return old_descriptors_->GetFieldType(descriptor);
}

Handle<FieldType> MapUpdater::GetOrComputeFieldType(
    InternalIndex descriptor, PropertyLocation location,
    Representation representation) const {
  DCHECK(descriptor.is_found());
  // |location| is just a pre-fetched GetDetails(descriptor).location().
  DCHECK_EQ(location, GetDetails(descriptor).location());
  if (location == kField) {
    return handle(GetFieldType(descriptor), isolate_);
  } else {
    return GetValue(descriptor).OptimalType(isolate_, representation);
  }
}

Handle<FieldType> MapUpdater::GetOrComputeFieldType(
    Handle<DescriptorArray> descriptors, InternalIndex descriptor,
    PropertyLocation location, Representation representation) {
  // |location| is just a pre-fetched GetDetails(descriptor).location().
  DCHECK_EQ(descriptors->GetDetails(descriptor).location(), location);
  if (location == kField) {
    return handle(descriptors->GetFieldType(descriptor), isolate_);
  } else {
    return descriptors->GetStrongValue(descriptor)
        .OptimalType(isolate_, representation);
  }
}

Handle<Map> MapUpdater::ReconfigureToDataField(InternalIndex descriptor,
                                               PropertyAttributes attributes,
                                               PropertyConstness constness,
                                               Representation representation,
                                               Handle<FieldType> field_type) {
  DCHECK_EQ(kInitialized, state_);
  DCHECK(descriptor.is_found());
  DCHECK(!old_map_->is_dictionary_map());

  base::SharedMutexGuard<base::kExclusive> mutex_guard(
      isolate_->map_updater_access());

  modified_descriptor_ = descriptor;
  new_kind_ = kData;
  new_attributes_ = attributes;
  new_location_ = kField;

  PropertyDetails old_details =
      old_descriptors_->GetDetails(modified_descriptor_);

  // If the {descriptor} was "const" data field so far, we need to update the
  // {old_map_} here, otherwise we could get the constants wrong, i.e.
  //
  //   o.x = 1;
  //   change o.x's attributes to something else
  //   delete o.x;
  //   o.x = 2;
  //
  // could trick V8 into thinking that `o.x` is still 1 even after the second
  // assignment.
  // This situation is similar to what might happen with property deletion.
  if (old_details.constness() == PropertyConstness::kConst &&
      old_details.location() == kField &&
      old_details.attributes() != new_attributes_) {
    // Ensure we'll be updating constness of the up-to-date version of old_map_.
    Handle<Map> old_map = UpdateMapNoLock(isolate_, old_map_);
    PropertyDetails details =
        old_map->instance_descriptors(isolate_).GetDetails(descriptor);
    Handle<FieldType> field_type(
        old_map->instance_descriptors(isolate_).GetFieldType(descriptor),
        isolate_);
    GeneralizeField(isolate_, old_map, descriptor, PropertyConstness::kMutable,
                    details.representation(), field_type);
    DCHECK_EQ(PropertyConstness::kMutable,
              old_map->instance_descriptors(isolate_)
                  .GetDetails(descriptor)
                  .constness());
    // The old_map_'s property must become mutable.
    // Note, that the {old_map_} and {old_descriptors_} are not expected to be
    // updated by the generalization if the map is already deprecated.
    DCHECK_IMPLIES(
        !old_map_->is_deprecated(),
        PropertyConstness::kMutable ==
            old_descriptors_->GetDetails(modified_descriptor_).constness());
    // Although the property in the old map is marked as mutable we still
    // treat it as constant when merging with the new path in transition tree.
    // This is fine because up until this reconfiguration the field was
    // known to be constant, so it's fair to proceed treating it as such
    // during this reconfiguration session. The issue is that after the
    // reconfiguration the original field might become mutable (see the delete
    // example above).
  }

  // If property kind is not reconfigured merge the result with
  // representation/field type from the old descriptor.
  if (old_details.kind() == new_kind_) {
    new_constness_ = GeneralizeConstness(constness, old_details.constness());

    Representation old_representation = old_details.representation();
    new_representation_ = representation.generalize(old_representation);

    Handle<FieldType> old_field_type =
        GetOrComputeFieldType(old_descriptors_, modified_descriptor_,
                              old_details.location(), new_representation_);

    new_field_type_ =
        GeneralizeFieldType(old_representation, old_field_type,
                            new_representation_, field_type, isolate_);
  } else {
    // We don't know if this is a first property kind reconfiguration
    // and we don't know which value was in this property previously
    // therefore we can't treat such a property as constant.
    new_constness_ = PropertyConstness::kMutable;
    new_representation_ = representation;
    new_field_type_ = field_type;
  }

  Map::GeneralizeIfCanHaveTransitionableFastElementsKind(
      isolate_, old_map_->instance_type(), &new_representation_,
      &new_field_type_);

  if (TryReconfigureToDataFieldInplace() == kEnd) return result_map_;
  if (FindRootMap() == kEnd) return result_map_;
  if (FindTargetMap() == kEnd) return result_map_;
  if (ConstructNewMap() == kAtIntegrityLevelSource) {
    ConstructNewMapWithIntegrityLevelTransition();
  }
  DCHECK_EQ(kEnd, state_);
  return result_map_;
}

Handle<Map> MapUpdater::ReconfigureElementsKind(ElementsKind elements_kind) {
  DCHECK_EQ(kInitialized, state_);

  base::SharedMutexGuard<base::kExclusive> mutex_guard(
      isolate_->map_updater_access());

  new_elements_kind_ = elements_kind;
  is_transitionable_fast_elements_kind_ =
      IsTransitionableFastElementsKind(new_elements_kind_);

  if (FindRootMap() == kEnd) return result_map_;
  if (FindTargetMap() == kEnd) return result_map_;
  if (ConstructNewMap() == kAtIntegrityLevelSource) {
    ConstructNewMapWithIntegrityLevelTransition();
  }
  DCHECK_EQ(kEnd, state_);
  return result_map_;
}

// static
Handle<Map> MapUpdater::UpdateMapNoLock(Isolate* isolate, Handle<Map> map) {
  if (!map->is_deprecated()) return map;
  // TODO(ishell): support fast map updating if we enable it.
  CHECK(!FLAG_fast_map_update);
  MapUpdater mu(isolate, map);
  // Update map without locking the Isolate::map_updater_access mutex.
  return mu.UpdateImpl();
}

Handle<Map> MapUpdater::Update() {
  base::SharedMutexGuard<base::kExclusive> mutex_guard(
      isolate_->map_updater_access());
  return UpdateImpl();
}

Handle<Map> MapUpdater::UpdateImpl() {
  DCHECK_EQ(kInitialized, state_);
  DCHECK(old_map_->is_deprecated());

  if (FindRootMap() == kEnd) return result_map_;
  if (FindTargetMap() == kEnd) return result_map_;
  if (ConstructNewMap() == kAtIntegrityLevelSource) {
    ConstructNewMapWithIntegrityLevelTransition();
  }
  DCHECK_EQ(kEnd, state_);
  if (FLAG_fast_map_update) {
    TransitionsAccessor(isolate_, old_map_).SetMigrationTarget(*result_map_);
  }
  return result_map_;
}

void MapUpdater::GeneralizeField(Handle<Map> map, InternalIndex modify_index,
                                 PropertyConstness new_constness,
                                 Representation new_representation,
                                 Handle<FieldType> new_field_type) {
  GeneralizeField(isolate_, map, modify_index, new_constness,
                  new_representation, new_field_type);

  DCHECK(*old_descriptors_ == old_map_->instance_descriptors(isolate_) ||
         *old_descriptors_ ==
             integrity_source_map_->instance_descriptors(isolate_));
}

MapUpdater::State MapUpdater::Normalize(const char* reason) {
  result_map_ = Map::Normalize(isolate_, old_map_, new_elements_kind_,
                               CLEAR_INOBJECT_PROPERTIES, reason);
  state_ = kEnd;
  return state_;  // Done.
}

MapUpdater::State MapUpdater::TryReconfigureToDataFieldInplace() {
  // Updating deprecated maps in-place doesn't make sense.
  if (old_map_->is_deprecated()) return state_;

  if (new_representation_.IsNone()) return state_;  // Not done yet.

  PropertyDetails old_details =
      old_descriptors_->GetDetails(modified_descriptor_);

  if (old_details.attributes() != new_attributes_ ||
      old_details.kind() != new_kind_ ||
      old_details.location() != new_location_) {
    // These changes can't be done in-place.
    return state_;  // Not done yet.
  }

  Representation old_representation = old_details.representation();
  if (!old_representation.CanBeInPlaceChangedTo(new_representation_)) {
    return state_;  // Not done yet.
  }

  DCHECK_EQ(new_kind_, old_details.kind());
  DCHECK_EQ(new_attributes_, old_details.attributes());
  DCHECK_EQ(kField, old_details.location());
  if (FLAG_trace_generalization) {
    PrintGeneralization(
        isolate_, old_map_, stdout, "uninitialized field", modified_descriptor_,
        old_nof_, old_nof_, false, old_representation, new_representation_,
        old_details.constness(), new_constness_,
        handle(old_descriptors_->GetFieldType(modified_descriptor_), isolate_),
        MaybeHandle<Object>(), new_field_type_, MaybeHandle<Object>());
  }
  GeneralizeField(old_map_, modified_descriptor_, new_constness_,
                  new_representation_, new_field_type_);
  // Check that the descriptor array was updated.
  DCHECK(old_descriptors_->GetDetails(modified_descriptor_)
             .representation()
             .Equals(new_representation_));
  DCHECK(old_descriptors_->GetFieldType(modified_descriptor_)
             .NowIs(new_field_type_));

  result_map_ = old_map_;
  state_ = kEnd;
  return state_;  // Done.
}

bool MapUpdater::TrySaveIntegrityLevelTransitions() {
  // Figure out the most restrictive integrity level transition (it should
  // be the last one in the transition tree).
  Handle<Map> previous =
      handle(Map::cast(old_map_->GetBackPointer()), isolate_);
  Symbol integrity_level_symbol;
  TransitionsAccessor last_transitions(isolate_, previous);
  if (!last_transitions.HasIntegrityLevelTransitionTo(
          *old_map_, &integrity_level_symbol, &integrity_level_)) {
    // The last transition was not integrity level transition - just bail out.
    // This can happen in the following cases:
    // - there are private symbol transitions following the integrity level
    //   transitions (see crbug.com/v8/8854).
    // - there is a getter added in addition to an existing setter (or a setter
    //   in addition to an existing getter).
    return false;
  }
  integrity_level_symbol_ = handle(integrity_level_symbol, isolate_);
  integrity_source_map_ = previous;

  // Now walk up the back pointer chain and skip all integrity level
  // transitions. If we encounter any non-integrity level transition interleaved
  // with integrity level transitions, just bail out.
  while (!integrity_source_map_->is_extensible()) {
    previous =
        handle(Map::cast(integrity_source_map_->GetBackPointer()), isolate_);
    TransitionsAccessor transitions(isolate_, previous);
    if (!transitions.HasIntegrityLevelTransitionTo(*integrity_source_map_)) {
      return false;
    }
    integrity_source_map_ = previous;
  }

  // Integrity-level transitions never change number of descriptors.
  CHECK_EQ(old_map_->NumberOfOwnDescriptors(),
           integrity_source_map_->NumberOfOwnDescriptors());

  has_integrity_level_transition_ = true;
  old_descriptors_ =
      handle(integrity_source_map_->instance_descriptors(isolate_), isolate_);
  return true;
}

MapUpdater::State MapUpdater::FindRootMap() {
  DCHECK_EQ(kInitialized, state_);
  // Check the state of the root map.
  root_map_ = handle(old_map_->FindRootMap(isolate_), isolate_);
  ElementsKind from_kind = root_map_->elements_kind();
  ElementsKind to_kind = new_elements_kind_;

  if (root_map_->is_deprecated()) {
    state_ = kEnd;
    result_map_ = handle(
        JSFunction::cast(root_map_->GetConstructor()).initial_map(), isolate_);
    result_map_ = Map::AsElementsKind(isolate_, result_map_, to_kind);
    DCHECK(result_map_->is_dictionary_map());
    return state_;
  }

  if (!old_map_->EquivalentToForTransition(*root_map_)) {
    return Normalize("Normalize_NotEquivalent");
  } else if (old_map_->is_extensible() != root_map_->is_extensible()) {
    DCHECK(!old_map_->is_extensible());
    DCHECK(root_map_->is_extensible());
    // We have an integrity level transition in the tree, let us make a note
    // of that transition to be able to replay it later.
    if (!TrySaveIntegrityLevelTransitions()) {
      return Normalize("Normalize_PrivateSymbolsOnNonExtensible");
    }

    // We want to build transitions to the original element kind (before
    // the seal transitions), so change {to_kind} accordingly.
    DCHECK(to_kind == DICTIONARY_ELEMENTS ||
           to_kind == SLOW_STRING_WRAPPER_ELEMENTS ||
           IsTypedArrayElementsKind(to_kind) ||
           IsAnyNonextensibleElementsKind(to_kind));
    to_kind = integrity_source_map_->elements_kind();
  }

  // TODO(ishell): Add a test for SLOW_SLOPPY_ARGUMENTS_ELEMENTS.
  if (from_kind != to_kind && to_kind != DICTIONARY_ELEMENTS &&
      to_kind != SLOW_STRING_WRAPPER_ELEMENTS &&
      to_kind != SLOW_SLOPPY_ARGUMENTS_ELEMENTS &&
      !(IsTransitionableFastElementsKind(from_kind) &&
        IsMoreGeneralElementsKindTransition(from_kind, to_kind))) {
    return Normalize("Normalize_InvalidElementsTransition");
  }

  int root_nof = root_map_->NumberOfOwnDescriptors();
  if (modified_descriptor_.is_found() &&
      modified_descriptor_.as_int() < root_nof) {
    PropertyDetails old_details =
        old_descriptors_->GetDetails(modified_descriptor_);
    if (old_details.kind() != new_kind_ ||
        old_details.attributes() != new_attributes_) {
      return Normalize("Normalize_RootModification1");
    }
    if (old_details.location() != kField) {
      return Normalize("Normalize_RootModification2");
    }
    if (!new_representation_.fits_into(old_details.representation())) {
      return Normalize("Normalize_RootModification4");
    }

    DCHECK_EQ(kData, old_details.kind());
    DCHECK_EQ(kData, new_kind_);
    DCHECK_EQ(kField, new_location_);

    // Modify root map in-place. The GeneralizeField method is a no-op
    // if the {old_map_} is already general enough to hold the requested
    // {new_constness_} and {new_field_type_}.
    GeneralizeField(old_map_, modified_descriptor_, new_constness_,
                    old_details.representation(), new_field_type_);
  }

  // From here on, use the map with correct elements kind as root map.
  root_map_ = Map::AsElementsKind(isolate_, root_map_, to_kind);
  state_ = kAtRootMap;
  return state_;  // Not done yet.
}

MapUpdater::State MapUpdater::FindTargetMap() {
  DCHECK_EQ(kAtRootMap, state_);
  target_map_ = root_map_;

  int root_nof = root_map_->NumberOfOwnDescriptors();
  for (InternalIndex i : InternalIndex::Range(root_nof, old_nof_)) {
    PropertyDetails old_details = GetDetails(i);
    Map transition = TransitionsAccessor(isolate_, target_map_)
                         .SearchTransition(GetKey(i), old_details.kind(),
                                           old_details.attributes());
    if (transition.is_null()) break;
    Handle<Map> tmp_map(transition, isolate_);

    Handle<DescriptorArray> tmp_descriptors(
        tmp_map->instance_descriptors(isolate_), isolate_);

    // Check if target map is incompatible.
    PropertyDetails tmp_details = tmp_descriptors->GetDetails(i);
    DCHECK_EQ(old_details.kind(), tmp_details.kind());
    DCHECK_EQ(old_details.attributes(), tmp_details.attributes());
    if (old_details.kind() == kAccessor &&
        !EqualImmutableValues(GetValue(i),
                              tmp_descriptors->GetStrongValue(i))) {
      // TODO(ishell): mutable accessors are not implemented yet.
      return Normalize("Normalize_Incompatible");
    }
    if (!IsGeneralizableTo(old_details.location(), tmp_details.location())) {
      break;
    }
    Representation tmp_representation = tmp_details.representation();
    if (!old_details.representation().fits_into(tmp_representation)) {
      // Try updating the field in-place to a generalized type.
      Representation generalized =
          tmp_representation.generalize(old_details.representation());
      if (!tmp_representation.CanBeInPlaceChangedTo(generalized)) {
        break;
      }
      tmp_representation = generalized;
    }

    if (tmp_details.location() == kField) {
      Handle<FieldType> old_field_type =
          GetOrComputeFieldType(i, old_details.location(), tmp_representation);
      GeneralizeField(tmp_map, i, old_details.constness(), tmp_representation,
                      old_field_type);
    } else {
      // kDescriptor: Check that the value matches.
      if (!EqualImmutableValues(GetValue(i),
                                tmp_descriptors->GetStrongValue(i))) {
        break;
      }
    }
    DCHECK(!tmp_map->is_deprecated());
    target_map_ = tmp_map;
  }

  // Directly change the map if the target map is more general.
  int target_nof = target_map_->NumberOfOwnDescriptors();
  if (target_nof == old_nof_) {
#ifdef DEBUG
    if (modified_descriptor_.is_found()) {
      DescriptorArray target_descriptors =
          target_map_->instance_descriptors(isolate_);
      PropertyDetails details =
          target_descriptors.GetDetails(modified_descriptor_);
      DCHECK_EQ(new_kind_, details.kind());
      DCHECK_EQ(GetDetails(modified_descriptor_).attributes(),
                details.attributes());
      DCHECK(IsGeneralizableTo(new_constness_, details.constness()));
      DCHECK_EQ(new_location_, details.location());
      DCHECK(new_representation_.fits_into(details.representation()));
      if (new_location_ == kField) {
        DCHECK_EQ(kField, details.location());
        DCHECK(new_field_type_->NowIs(
            target_descriptors.GetFieldType(modified_descriptor_)));
      } else {
        DCHECK(details.location() == kField ||
               EqualImmutableValues(
                   *new_value_,
                   target_descriptors.GetStrongValue(modified_descriptor_)));
      }
    }
#endif
    if (*target_map_ != *old_map_) {
      old_map_->NotifyLeafMapLayoutChange(isolate_);
    }
    if (!has_integrity_level_transition_) {
      result_map_ = target_map_;
      state_ = kEnd;
      return state_;  // Done.
    }

    // We try to replay the integrity level transition here.
    Map transition = TransitionsAccessor(isolate_, target_map_)
                         .SearchSpecial(*integrity_level_symbol_);
    if (!transition.is_null()) {
      result_map_ = handle(transition, isolate_);
      state_ = kEnd;
      return state_;  // Done.
    }
  }

  // Find the last compatible target map in the transition tree.
  for (InternalIndex i : InternalIndex::Range(target_nof, old_nof_)) {
    PropertyDetails old_details = GetDetails(i);
    Map transition = TransitionsAccessor(isolate_, target_map_)
                         .SearchTransition(GetKey(i), old_details.kind(),
                                           old_details.attributes());
    if (transition.is_null()) break;
    Handle<Map> tmp_map(transition, isolate_);
    Handle<DescriptorArray> tmp_descriptors(
        tmp_map->instance_descriptors(isolate_), isolate_);
#ifdef DEBUG
    // Check that target map is compatible.
    PropertyDetails tmp_details = tmp_descriptors->GetDetails(i);
    DCHECK_EQ(old_details.kind(), tmp_details.kind());
    DCHECK_EQ(old_details.attributes(), tmp_details.attributes());
#endif
    if (old_details.kind() == kAccessor &&
        !EqualImmutableValues(GetValue(i),
                              tmp_descriptors->GetStrongValue(i))) {
      return Normalize("Normalize_Incompatible");
    }
    DCHECK(!tmp_map->is_deprecated());
    target_map_ = tmp_map;
  }

  state_ = kAtTargetMap;
  return state_;  // Not done yet.
}

Handle<DescriptorArray> MapUpdater::BuildDescriptorArray() {
  InstanceType instance_type = old_map_->instance_type();
  int target_nof = target_map_->NumberOfOwnDescriptors();
  Handle<DescriptorArray> target_descriptors(
      target_map_->instance_descriptors(isolate_), isolate_);

  // Allocate a new descriptor array large enough to hold the required
  // descriptors, with minimally the exact same size as the old descriptor
  // array.
  int new_slack =
      std::max<int>(old_nof_, old_descriptors_->number_of_descriptors()) -
      old_nof_;
  Handle<DescriptorArray> new_descriptors =
      DescriptorArray::Allocate(isolate_, old_nof_, new_slack);
  DCHECK(new_descriptors->number_of_all_descriptors() >
             target_descriptors->number_of_all_descriptors() ||
         new_descriptors->number_of_slack_descriptors() > 0 ||
         new_descriptors->number_of_descriptors() ==
             old_descriptors_->number_of_descriptors());
  DCHECK(new_descriptors->number_of_descriptors() == old_nof_);

  int root_nof = root_map_->NumberOfOwnDescriptors();

  // Given that we passed root modification check in FindRootMap() so
  // the root descriptors are either not modified at all or already more
  // general than we requested. Take |root_nof| entries as is.
  // 0 -> |root_nof|
  int current_offset = 0;
  for (InternalIndex i : InternalIndex::Range(root_nof)) {
    PropertyDetails old_details = old_descriptors_->GetDetails(i);
    if (old_details.location() == kField) {
      current_offset += old_details.field_width_in_words();
    }
    Descriptor d(handle(GetKey(i), isolate_),
                 MaybeObjectHandle(old_descriptors_->GetValue(i), isolate_),
                 old_details);
    new_descriptors->Set(i, &d);
  }

  // Merge "updated" old_descriptor entries with target_descriptor entries.
  // |root_nof| -> |target_nof|
  for (InternalIndex i : InternalIndex::Range(root_nof, target_nof)) {
    Handle<Name> key(GetKey(i), isolate_);
    PropertyDetails old_details = GetDetails(i);
    PropertyDetails target_details = target_descriptors->GetDetails(i);

    PropertyKind next_kind = old_details.kind();
    PropertyAttributes next_attributes = old_details.attributes();
    DCHECK_EQ(next_kind, target_details.kind());
    DCHECK_EQ(next_attributes, target_details.attributes());

    PropertyConstness next_constness = GeneralizeConstness(
        old_details.constness(), target_details.constness());

    // Note: failed values equality check does not invalidate per-object
    // property constness.
    PropertyLocation next_location =
        old_details.location() == kField ||
                target_details.location() == kField ||
                !EqualImmutableValues(target_descriptors->GetStrongValue(i),
                                      GetValue(i))
            ? kField
            : kDescriptor;

    // Ensure that mutable values are stored in fields.
    DCHECK_IMPLIES(next_constness == PropertyConstness::kMutable,
                   next_location == kField);

    Representation next_representation =
        old_details.representation().generalize(
            target_details.representation());

    if (next_location == kField) {
      Handle<FieldType> old_field_type =
          GetOrComputeFieldType(i, old_details.location(), next_representation);

      Handle<FieldType> target_field_type =
          GetOrComputeFieldType(target_descriptors, i,
                                target_details.location(), next_representation);

      Handle<FieldType> next_field_type =
          GeneralizeFieldType(old_details.representation(), old_field_type,
                              next_representation, target_field_type, isolate_);

      Map::GeneralizeIfCanHaveTransitionableFastElementsKind(
          isolate_, instance_type, &next_representation, &next_field_type);

      MaybeObjectHandle wrapped_type(
          Map::WrapFieldType(isolate_, next_field_type));
      Descriptor d;
      if (next_kind == kData) {
        d = Descriptor::DataField(key, current_offset, next_attributes,
                                  next_constness, next_representation,
                                  wrapped_type);
      } else {
        // TODO(ishell): mutable accessors are not implemented yet.
        UNIMPLEMENTED();
      }
      current_offset += d.GetDetails().field_width_in_words();
      new_descriptors->Set(i, &d);
    } else {
      DCHECK_EQ(kDescriptor, next_location);
      DCHECK_EQ(PropertyConstness::kConst, next_constness);

      Handle<Object> value(GetValue(i), isolate_);
      DCHECK_EQ(kAccessor, next_kind);
      Descriptor d = Descriptor::AccessorConstant(key, value, next_attributes);
      new_descriptors->Set(i, &d);
    }
  }

  // Take "updated" old_descriptor entries.
  // |target_nof| -> |old_nof|
  for (InternalIndex i : InternalIndex::Range(target_nof, old_nof_)) {
    PropertyDetails old_details = GetDetails(i);
    Handle<Name> key(GetKey(i), isolate_);

    PropertyKind next_kind = old_details.kind();
    PropertyAttributes next_attributes = old_details.attributes();
    PropertyConstness next_constness = old_details.constness();
    PropertyLocation next_location = old_details.location();
    Representation next_representation = old_details.representation();

    Descriptor d;
    if (next_location == kField) {
      Handle<FieldType> next_field_type =
          GetOrComputeFieldType(i, old_details.location(), next_representation);

      // If the |new_elements_kind_| is still transitionable then the old map's
      // elements kind is also transitionable and therefore the old descriptors
      // array must already have generalized field type.
      CHECK_IMPLIES(
          is_transitionable_fast_elements_kind_,
          Map::IsMostGeneralFieldType(next_representation, *next_field_type));

      MaybeObjectHandle wrapped_type(
          Map::WrapFieldType(isolate_, next_field_type));
      Descriptor d;
      if (next_kind == kData) {
        d = Descriptor::DataField(key, current_offset, next_attributes,
                                  next_constness, next_representation,
                                  wrapped_type);
      } else {
        // TODO(ishell): mutable accessors are not implemented yet.
        UNIMPLEMENTED();
      }
      current_offset += d.GetDetails().field_width_in_words();
      new_descriptors->Set(i, &d);
    } else {
      DCHECK_EQ(kDescriptor, next_location);
      DCHECK_EQ(PropertyConstness::kConst, next_constness);

      Handle<Object> value(GetValue(i), isolate_);
      if (next_kind == kData) {
        d = Descriptor::DataConstant(key, value, next_attributes);
      } else {
        DCHECK_EQ(kAccessor, next_kind);
        d = Descriptor::AccessorConstant(key, value, next_attributes);
      }
      new_descriptors->Set(i, &d);
    }
  }

  new_descriptors->Sort();
  return new_descriptors;
}

Handle<Map> MapUpdater::FindSplitMap(Handle<DescriptorArray> descriptors) {
  DisallowGarbageCollection no_gc;

  int root_nof = root_map_->NumberOfOwnDescriptors();
  Map current = *root_map_;
  for (InternalIndex i : InternalIndex::Range(root_nof, old_nof_)) {
    Name name = descriptors->GetKey(i);
    PropertyDetails details = descriptors->GetDetails(i);
    Map next =
        TransitionsAccessor(isolate_, current, &no_gc)
            .SearchTransition(name, details.kind(), details.attributes());
    if (next.is_null()) break;
    DescriptorArray next_descriptors = next.instance_descriptors(isolate_);

    PropertyDetails next_details = next_descriptors.GetDetails(i);
    DCHECK_EQ(details.kind(), next_details.kind());
    DCHECK_EQ(details.attributes(), next_details.attributes());
    if (details.constness() != next_details.constness()) break;
    if (details.location() != next_details.location()) break;
    if (!details.representation().Equals(next_details.representation())) break;

    if (next_details.location() == kField) {
      FieldType next_field_type = next_descriptors.GetFieldType(i);
      if (!descriptors->GetFieldType(i).NowIs(next_field_type)) {
        break;
      }
    } else {
      if (!EqualImmutableValues(descriptors->GetStrongValue(i),
                                next_descriptors.GetStrongValue(i))) {
        break;
      }
    }
    current = next;
  }
  return handle(current, isolate_);
}

MapUpdater::State MapUpdater::ConstructNewMap() {
  Handle<DescriptorArray> new_descriptors = BuildDescriptorArray();

  Handle<Map> split_map = FindSplitMap(new_descriptors);
  int split_nof = split_map->NumberOfOwnDescriptors();
  if (old_nof_ == split_nof) {
    CHECK(has_integrity_level_transition_);
    state_ = kAtIntegrityLevelSource;
    return state_;
  }
  InternalIndex split_index(split_nof);
  PropertyDetails split_details = GetDetails(split_index);
  TransitionsAccessor transitions(isolate_, split_map);

  // Invalidate a transition target at |key|.
  Handle<Map> maybe_transition(
      transitions.SearchTransition(GetKey(split_index), split_details.kind(),
                                   split_details.attributes()),
      isolate_);
  if (!maybe_transition->is_null()) {
    maybe_transition->DeprecateTransitionTree(isolate_);
  }

  // If |maybe_transition| is not nullptr then the transition array already
  // contains entry for given descriptor. This means that the transition
  // could be inserted regardless of whether transitions array is full or not.
  if (maybe_transition->is_null() && !transitions.CanHaveMoreTransitions()) {
    return Normalize("Normalize_CantHaveMoreTransitions");
  }

  old_map_->NotifyLeafMapLayoutChange(isolate_);

  if (FLAG_trace_generalization && modified_descriptor_.is_found()) {
    PropertyDetails old_details =
        old_descriptors_->GetDetails(modified_descriptor_);
    PropertyDetails new_details =
        new_descriptors->GetDetails(modified_descriptor_);
    MaybeHandle<FieldType> old_field_type;
    MaybeHandle<FieldType> new_field_type;
    MaybeHandle<Object> old_value;
    MaybeHandle<Object> new_value;
    if (old_details.location() == kField) {
      old_field_type = handle(
          old_descriptors_->GetFieldType(modified_descriptor_), isolate_);
    } else {
      old_value = handle(old_descriptors_->GetStrongValue(modified_descriptor_),
                         isolate_);
    }
    if (new_details.location() == kField) {
      new_field_type =
          handle(new_descriptors->GetFieldType(modified_descriptor_), isolate_);
    } else {
      new_value = handle(new_descriptors->GetStrongValue(modified_descriptor_),
                         isolate_);
    }

    PrintGeneralization(
        isolate_, old_map_, stdout, "", modified_descriptor_, split_nof,
        old_nof_,
        old_details.location() == kDescriptor && new_location_ == kField,
        old_details.representation(), new_details.representation(),
        old_details.constness(), new_details.constness(), old_field_type,
        old_value, new_field_type, new_value);
  }

  Handle<Map> new_map =
      Map::AddMissingTransitions(isolate_, split_map, new_descriptors);

  // Deprecated part of the transition tree is no longer reachable, so replace
  // current instance descriptors in the "survived" part of the tree with
  // the new descriptors to maintain descriptors sharing invariant.
  split_map->ReplaceDescriptors(isolate_, *new_descriptors);

  if (has_integrity_level_transition_) {
    target_map_ = new_map;
    state_ = kAtIntegrityLevelSource;
  } else {
    result_map_ = new_map;
    state_ = kEnd;
  }
  return state_;  // Done.
}

MapUpdater::State MapUpdater::ConstructNewMapWithIntegrityLevelTransition() {
  DCHECK_EQ(kAtIntegrityLevelSource, state_);

  TransitionsAccessor transitions(isolate_, target_map_);
  if (!transitions.CanHaveMoreTransitions()) {
    return Normalize("Normalize_CantHaveMoreTransitions");
  }

  result_map_ = Map::CopyForPreventExtensions(
      isolate_, target_map_, integrity_level_, integrity_level_symbol_,
      "CopyForPreventExtensions",
      old_map_->elements_kind() == DICTIONARY_ELEMENTS);
  DCHECK_IMPLIES(old_map_->elements_kind() == DICTIONARY_ELEMENTS,
                 result_map_->elements_kind() == DICTIONARY_ELEMENTS);

  state_ = kEnd;
  return state_;
}

namespace {

void PrintReconfiguration(Isolate* isolate, Handle<Map> map, FILE* file,
                          InternalIndex modify_index, PropertyKind kind,
                          PropertyAttributes attributes) {
  OFStream os(file);
  os << "[reconfiguring]";
  Name name = map->instance_descriptors(isolate).GetKey(modify_index);
  if (name.IsString()) {
    String::cast(name).PrintOn(file);
  } else {
    os << "{symbol " << reinterpret_cast<void*>(name.ptr()) << "}";
  }
  os << ": " << (kind == kData ? "kData" : "ACCESSORS") << ", attrs: ";
  os << attributes << " [";
  JavaScriptFrame::PrintTop(isolate, file, false, true);
  os << "]\n";
}

}  // namespace

// static
Handle<Map> MapUpdater::ReconfigureExistingProperty(
    Isolate* isolate, Handle<Map> map, InternalIndex descriptor,
    PropertyKind kind, PropertyAttributes attributes,
    PropertyConstness constness) {
  // Dictionaries have to be reconfigured in-place.
  DCHECK(!map->is_dictionary_map());
  DCHECK_EQ(kData, kind);  // Only kData case is supported so far.

  if (!map->GetBackPointer().IsMap()) {
    // There is no benefit from reconstructing transition tree for maps without
    // back pointers, normalize and try to hit the map cache instead.
    return Map::Normalize(isolate, map, CLEAR_INOBJECT_PROPERTIES,
                          "Normalize_AttributesMismatchProtoMap");
  }

  if (FLAG_trace_generalization) {
    PrintReconfiguration(isolate, map, stdout, descriptor, kind, attributes);
  }

  return MapUpdater{isolate, map}.ReconfigureToDataField(
      descriptor, attributes, constness, Representation::None(),
      FieldType::None(isolate));
}

// static
void MapUpdater::UpdateFieldType(Isolate* isolate, Handle<Map> map,
                                 InternalIndex descriptor, Handle<Name> name,
                                 PropertyConstness new_constness,
                                 Representation new_representation,
                                 const MaybeObjectHandle& new_wrapped_type) {
  DCHECK(new_wrapped_type->IsSmi() || new_wrapped_type->IsWeak());
  // We store raw pointers in the queue, so no allocations are allowed.
  DisallowGarbageCollection no_gc;
  PropertyDetails details =
      map->instance_descriptors(isolate).GetDetails(descriptor);
  if (details.location() != kField) return;
  DCHECK_EQ(kData, details.kind());

  if (new_constness != details.constness() && map->is_prototype_map()) {
    JSObject::InvalidatePrototypeChains(*map);
  }

  std::queue<Map> backlog;
  backlog.push(*map);

  while (!backlog.empty()) {
    Map current = backlog.front();
    backlog.pop();

    TransitionsAccessor transitions(isolate, current, &no_gc);
    int num_transitions = transitions.NumberOfTransitions();
    for (int i = 0; i < num_transitions; ++i) {
      Map target = transitions.GetTarget(i);
      backlog.push(target);
    }
    DescriptorArray descriptors = current.instance_descriptors(isolate);
    PropertyDetails details = descriptors.GetDetails(descriptor);

    // It is allowed to change representation here only from None
    // to something or from Smi or HeapObject to Tagged.
    DCHECK(details.representation().Equals(new_representation) ||
           details.representation().CanBeInPlaceChangedTo(new_representation));

    // Skip if already updated the shared descriptor.
    if (new_constness != details.constness() ||
        !new_representation.Equals(details.representation()) ||
        descriptors.GetFieldType(descriptor) != *new_wrapped_type.object()) {
      Descriptor d = Descriptor::DataField(
          name, descriptors.GetFieldIndex(descriptor), details.attributes(),
          new_constness, new_representation, new_wrapped_type);
      descriptors.Replace(descriptor, &d);
    }
  }
}

// TODO(jgruber): Lock the map-updater mutex.
// static
void MapUpdater::GeneralizeField(Isolate* isolate, Handle<Map> map,
                                 InternalIndex modify_index,
                                 PropertyConstness new_constness,
                                 Representation new_representation,
                                 Handle<FieldType> new_field_type) {
  // Check if we actually need to generalize the field type at all.
  Handle<DescriptorArray> old_descriptors(map->instance_descriptors(isolate),
                                          isolate);
  PropertyDetails old_details = old_descriptors->GetDetails(modify_index);
  PropertyConstness old_constness = old_details.constness();
  Representation old_representation = old_details.representation();
  Handle<FieldType> old_field_type(old_descriptors->GetFieldType(modify_index),
                                   isolate);

  // Return if the current map is general enough to hold requested constness and
  // representation/field type.
  if (IsGeneralizableTo(new_constness, old_constness) &&
      old_representation.Equals(new_representation) &&
      !Map::FieldTypeIsCleared(new_representation, *new_field_type) &&
      // Checking old_field_type for being cleared is not necessary because
      // the NowIs check below would fail anyway in that case.
      new_field_type->NowIs(old_field_type)) {
    DCHECK(GeneralizeFieldType(old_representation, old_field_type,
                               new_representation, new_field_type, isolate)
               ->NowIs(old_field_type));
    return;
  }

  // Determine the field owner.
  Handle<Map> field_owner(map->FindFieldOwner(isolate, modify_index), isolate);
  Handle<DescriptorArray> descriptors(
      field_owner->instance_descriptors(isolate), isolate);
  DCHECK_EQ(*old_field_type, descriptors->GetFieldType(modify_index));

  new_field_type =
      GeneralizeFieldType(old_representation, old_field_type,
                          new_representation, new_field_type, isolate);

  new_constness = GeneralizeConstness(old_constness, new_constness);

  PropertyDetails details = descriptors->GetDetails(modify_index);
  Handle<Name> name(descriptors->GetKey(modify_index), isolate);

  MaybeObjectHandle wrapped_type(Map::WrapFieldType(isolate, new_field_type));
  UpdateFieldType(isolate, field_owner, modify_index, name, new_constness,
                  new_representation, wrapped_type);

  if (new_constness != old_constness) {
    field_owner->dependent_code().DeoptimizeDependentCodeGroup(
        DependentCode::kFieldConstGroup);
  }

  if (!new_field_type->Equals(*old_field_type)) {
    field_owner->dependent_code().DeoptimizeDependentCodeGroup(
        DependentCode::kFieldTypeGroup);
  }

  if (!new_representation.Equals(old_representation)) {
    field_owner->dependent_code().DeoptimizeDependentCodeGroup(
        DependentCode::kFieldRepresentationGroup);
  }

  if (FLAG_trace_generalization) {
    PrintGeneralization(
        isolate, map, stdout, "field type generalization", modify_index,
        map->NumberOfOwnDescriptors(), map->NumberOfOwnDescriptors(), false,
        details.representation(),
        descriptors->GetDetails(modify_index).representation(), old_constness,
        new_constness, old_field_type, MaybeHandle<Object>(), new_field_type,
        MaybeHandle<Object>());
  }
}

}  // namespace internal
}  // namespace v8
