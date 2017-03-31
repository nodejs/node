// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/map-updater.h"

#include "src/field-type.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/transitions.h"

namespace v8 {
namespace internal {

namespace {

inline bool EqualImmutableValues(Object* obj1, Object* obj2) {
  if (obj1 == obj2) return true;  // Valid for both kData and kAccessor kinds.
  // TODO(ishell): compare AccessorPairs.
  return false;
}

inline bool LocationFitsInto(PropertyLocation what, PropertyLocation where) {
  return where == kField || what == kDescriptor;
}

}  // namespace

Name* MapUpdater::GetKey(int descriptor) const {
  return old_descriptors_->GetKey(descriptor);
}

PropertyDetails MapUpdater::GetDetails(int descriptor) const {
  DCHECK_LE(0, descriptor);
  if (descriptor == modified_descriptor_) {
    return PropertyDetails(new_kind_, new_attributes_, new_location_,
                           new_representation_);
  }
  return old_descriptors_->GetDetails(descriptor);
}

Object* MapUpdater::GetValue(int descriptor) const {
  DCHECK_LE(0, descriptor);
  if (descriptor == modified_descriptor_) {
    DCHECK_EQ(kDescriptor, new_location_);
    return *new_value_;
  }
  DCHECK_EQ(kDescriptor, GetDetails(descriptor).location());
  return old_descriptors_->GetValue(descriptor);
}

FieldType* MapUpdater::GetFieldType(int descriptor) const {
  DCHECK_LE(0, descriptor);
  if (descriptor == modified_descriptor_) {
    DCHECK_EQ(kField, new_location_);
    return *new_field_type_;
  }
  DCHECK_EQ(kField, GetDetails(descriptor).location());
  return old_descriptors_->GetFieldType(descriptor);
}

Handle<FieldType> MapUpdater::GetOrComputeFieldType(
    int descriptor, PropertyLocation location,
    Representation representation) const {
  DCHECK_LE(0, descriptor);
  // |location| is just a pre-fetched GetDetails(descriptor).location().
  DCHECK_EQ(location, GetDetails(descriptor).location());
  if (location == kField) {
    return handle(GetFieldType(descriptor), isolate_);
  } else {
    return GetValue(descriptor)->OptimalType(isolate_, representation);
  }
}

Handle<FieldType> MapUpdater::GetOrComputeFieldType(
    Handle<DescriptorArray> descriptors, int descriptor,
    PropertyLocation location, Representation representation) {
  // |location| is just a pre-fetched GetDetails(descriptor).location().
  DCHECK_EQ(descriptors->GetDetails(descriptor).location(), location);
  if (location == kField) {
    return handle(descriptors->GetFieldType(descriptor), isolate_);
  } else {
    return descriptors->GetValue(descriptor)
        ->OptimalType(isolate_, representation);
  }
}

Handle<Map> MapUpdater::ReconfigureToDataField(int descriptor,
                                               PropertyAttributes attributes,
                                               Representation representation,
                                               Handle<FieldType> field_type) {
  DCHECK_EQ(kInitialized, state_);
  DCHECK_LE(0, descriptor);
  DCHECK(!old_map_->is_dictionary_map());
  modified_descriptor_ = descriptor;
  new_kind_ = kData;
  new_attributes_ = attributes;
  new_location_ = kField;
  new_representation_ = representation;
  new_field_type_ = field_type;

  PropertyDetails old_details =
      old_descriptors_->GetDetails(modified_descriptor_);

  // If property kind is not reconfigured merge the result with
  // representation/field type from the old descriptor.
  if (old_details.kind() == new_kind_) {
    Representation old_representation = old_details.representation();
    new_representation_ = new_representation_.generalize(old_representation);

    Handle<FieldType> old_field_type =
        GetOrComputeFieldType(old_descriptors_, modified_descriptor_,
                              old_details.location(), new_representation_);

    new_field_type_ = Map::GeneralizeFieldType(
        old_representation, old_field_type, new_representation_,
        new_field_type_, isolate_);
  }

  if (TryRecofigureToDataFieldInplace() == kEnd) return result_map_;
  if (FindRootMap() == kEnd) return result_map_;
  if (FindTargetMap() == kEnd) return result_map_;
  ConstructNewMap();
  DCHECK_EQ(kEnd, state_);
  return result_map_;
}

Handle<Map> MapUpdater::ReconfigureElementsKind(ElementsKind elements_kind) {
  DCHECK_EQ(kInitialized, state_);
  new_elements_kind_ = elements_kind;

  if (FindRootMap() == kEnd) return result_map_;
  if (FindTargetMap() == kEnd) return result_map_;
  ConstructNewMap();
  DCHECK_EQ(kEnd, state_);
  return result_map_;
}

Handle<Map> MapUpdater::Update() {
  DCHECK_EQ(kInitialized, state_);
  DCHECK(old_map_->is_deprecated());

  if (FindRootMap() == kEnd) return result_map_;
  if (FindTargetMap() == kEnd) return result_map_;
  ConstructNewMap();
  DCHECK_EQ(kEnd, state_);
  return result_map_;
}

MapUpdater::State MapUpdater::CopyGeneralizeAllFields(const char* reason) {
  result_map_ = Map::CopyGeneralizeAllFields(old_map_, new_elements_kind_,
                                             modified_descriptor_, new_kind_,
                                             new_attributes_, reason);
  state_ = kEnd;
  return state_;  // Done.
}

MapUpdater::State MapUpdater::TryRecofigureToDataFieldInplace() {
  // If it's just a representation generalization case (i.e. property kind and
  // attributes stays unchanged) it's fine to transition from None to anything
  // but double without any modification to the object, because the default
  // uninitialized value for representation None can be overwritten by both
  // smi and tagged values. Doubles, however, would require a box allocation.
  if (new_representation_.IsNone() || new_representation_.IsDouble()) {
    return state_;  // Not done yet.
  }

  PropertyDetails old_details =
      old_descriptors_->GetDetails(modified_descriptor_);
  Representation old_representation = old_details.representation();
  if (!old_representation.IsNone()) {
    return state_;  // Not done yet.
  }

  DCHECK_EQ(new_kind_, old_details.kind());
  DCHECK_EQ(new_attributes_, old_details.attributes());
  DCHECK_EQ(kField, old_details.location());
  if (FLAG_trace_generalization) {
    old_map_->PrintGeneralization(
        stdout, "uninitialized field", modified_descriptor_, old_nof_, old_nof_,
        false, old_representation, new_representation_,
        handle(old_descriptors_->GetFieldType(modified_descriptor_), isolate_),
        MaybeHandle<Object>(), new_field_type_, MaybeHandle<Object>());
  }
  Handle<Map> field_owner(old_map_->FindFieldOwner(modified_descriptor_),
                          isolate_);

  Map::GeneralizeField(field_owner, modified_descriptor_, new_representation_,
                       new_field_type_);
  // Check that the descriptor array was updated.
  DCHECK(old_descriptors_->GetDetails(modified_descriptor_)
             .representation()
             .Equals(new_representation_));
  DCHECK(old_descriptors_->GetFieldType(modified_descriptor_)
             ->NowIs(new_field_type_));

  result_map_ = old_map_;
  state_ = kEnd;
  return state_;  // Done.
}

MapUpdater::State MapUpdater::FindRootMap() {
  DCHECK_EQ(kInitialized, state_);
  // Check the state of the root map.
  root_map_ = handle(old_map_->FindRootMap(), isolate_);
  int root_nof = root_map_->NumberOfOwnDescriptors();
  if (!old_map_->EquivalentToForTransition(*root_map_)) {
    return CopyGeneralizeAllFields("GenAll_NotEquivalent");
  }

  ElementsKind from_kind = root_map_->elements_kind();
  ElementsKind to_kind = new_elements_kind_;
  // TODO(ishell): Add a test for SLOW_SLOPPY_ARGUMENTS_ELEMENTS.
  if (from_kind != to_kind && to_kind != DICTIONARY_ELEMENTS &&
      to_kind != SLOW_STRING_WRAPPER_ELEMENTS &&
      to_kind != SLOW_SLOPPY_ARGUMENTS_ELEMENTS &&
      !(IsTransitionableFastElementsKind(from_kind) &&
        IsMoreGeneralElementsKindTransition(from_kind, to_kind))) {
    return CopyGeneralizeAllFields("GenAll_InvalidElementsTransition");
  }

  if (modified_descriptor_ >= 0 && modified_descriptor_ < root_nof) {
    PropertyDetails old_details =
        old_descriptors_->GetDetails(modified_descriptor_);
    if (old_details.kind() != new_kind_ ||
        old_details.attributes() != new_attributes_) {
      return CopyGeneralizeAllFields("GenAll_RootModification1");
    }
    if (!new_representation_.fits_into(old_details.representation())) {
      return CopyGeneralizeAllFields("GenAll_RootModification2");
    }
    if (old_details.location() != kField) {
      return CopyGeneralizeAllFields("GenAll_RootModification3");
    }
    DCHECK_EQ(kData, old_details.kind());
    DCHECK_EQ(kData, new_kind_);
    DCHECK_EQ(kField, new_location_);
    FieldType* old_field_type =
        old_descriptors_->GetFieldType(modified_descriptor_);
    if (!new_field_type_->NowIs(old_field_type)) {
      return CopyGeneralizeAllFields("GenAll_RootModification4");
    }
  }

  // From here on, use the map with correct elements kind as root map.
  if (from_kind != to_kind) {
    root_map_ = Map::AsElementsKind(root_map_, to_kind);
  }
  state_ = kAtRootMap;
  return state_;  // Not done yet.
}

MapUpdater::State MapUpdater::FindTargetMap() {
  DCHECK_EQ(kAtRootMap, state_);
  target_map_ = root_map_;

  int root_nof = root_map_->NumberOfOwnDescriptors();
  for (int i = root_nof; i < old_nof_; ++i) {
    PropertyDetails old_details = GetDetails(i);
    Map* transition = TransitionArray::SearchTransition(
        *target_map_, old_details.kind(), GetKey(i), old_details.attributes());
    if (transition == NULL) break;
    Handle<Map> tmp_map(transition, isolate_);

    Handle<DescriptorArray> tmp_descriptors(tmp_map->instance_descriptors(),
                                            isolate_);

    // Check if target map is incompatible.
    PropertyDetails tmp_details = tmp_descriptors->GetDetails(i);
    DCHECK_EQ(old_details.kind(), tmp_details.kind());
    DCHECK_EQ(old_details.attributes(), tmp_details.attributes());
    if (old_details.kind() == kAccessor &&
        !EqualImmutableValues(GetValue(i), tmp_descriptors->GetValue(i))) {
      // TODO(ishell): mutable accessors are not implemented yet.
      return CopyGeneralizeAllFields("GenAll_Incompatible");
    }
    // Check if old location fits into tmp location.
    if (!LocationFitsInto(old_details.location(), tmp_details.location())) {
      break;
    }

    // Check if old representation fits into tmp representation.
    Representation tmp_representation = tmp_details.representation();
    if (!old_details.representation().fits_into(tmp_representation)) {
      break;
    }

    if (tmp_details.location() == kField) {
      Handle<FieldType> old_field_type =
          GetOrComputeFieldType(i, old_details.location(), tmp_representation);
      Map::GeneralizeField(tmp_map, i, tmp_representation, old_field_type);
    } else {
      // kDescriptor: Check that the value matches.
      if (!EqualImmutableValues(GetValue(i), tmp_descriptors->GetValue(i))) {
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
    if (modified_descriptor_ >= 0) {
      DescriptorArray* target_descriptors = target_map_->instance_descriptors();
      PropertyDetails details =
          target_descriptors->GetDetails(modified_descriptor_);
      DCHECK_EQ(new_kind_, details.kind());
      DCHECK_EQ(new_attributes_, details.attributes());
      DCHECK_EQ(new_location_, details.location());
      DCHECK(new_representation_.fits_into(details.representation()));
      if (new_location_ == kField) {
        DCHECK_EQ(kField, details.location());
        DCHECK(new_field_type_->NowIs(
            target_descriptors->GetFieldType(modified_descriptor_)));
      } else {
        DCHECK(details.location() == kField ||
               EqualImmutableValues(*new_value_, target_descriptors->GetValue(
                                                     modified_descriptor_)));
      }
    }
#endif
    if (*target_map_ != *old_map_) {
      old_map_->NotifyLeafMapLayoutChange();
    }
    result_map_ = target_map_;
    state_ = kEnd;
    return state_;  // Done.
  }

  // Find the last compatible target map in the transition tree.
  for (int i = target_nof; i < old_nof_; ++i) {
    PropertyDetails old_details = GetDetails(i);
    Map* transition = TransitionArray::SearchTransition(
        *target_map_, old_details.kind(), GetKey(i), old_details.attributes());
    if (transition == NULL) break;
    Handle<Map> tmp_map(transition, isolate_);
    Handle<DescriptorArray> tmp_descriptors(tmp_map->instance_descriptors(),
                                            isolate_);

#ifdef DEBUG
    // Check that target map is compatible.
    PropertyDetails tmp_details = tmp_descriptors->GetDetails(i);
    DCHECK_EQ(old_details.kind(), tmp_details.kind());
    DCHECK_EQ(old_details.attributes(), tmp_details.attributes());
#endif
    if (old_details.kind() == kAccessor &&
        !EqualImmutableValues(GetValue(i), tmp_descriptors->GetValue(i))) {
      return CopyGeneralizeAllFields("GenAll_Incompatible");
    }
    DCHECK(!tmp_map->is_deprecated());
    target_map_ = tmp_map;
  }

  state_ = kAtTargetMap;
  return state_;  // Not done yet.
}

Handle<DescriptorArray> MapUpdater::BuildDescriptorArray() {
  int target_nof = target_map_->NumberOfOwnDescriptors();
  Handle<DescriptorArray> target_descriptors(
      target_map_->instance_descriptors(), isolate_);

  // Allocate a new descriptor array large enough to hold the required
  // descriptors, with minimally the exact same size as the old descriptor
  // array.
  int new_slack =
      Max(old_nof_, old_descriptors_->number_of_descriptors()) - old_nof_;
  Handle<DescriptorArray> new_descriptors =
      DescriptorArray::Allocate(isolate_, old_nof_, new_slack);
  DCHECK(new_descriptors->length() > target_descriptors->length() ||
         new_descriptors->NumberOfSlackDescriptors() > 0 ||
         new_descriptors->number_of_descriptors() ==
             old_descriptors_->number_of_descriptors());
  DCHECK(new_descriptors->number_of_descriptors() == old_nof_);

  int root_nof = root_map_->NumberOfOwnDescriptors();

  // Given that we passed root modification check in FindRootMap() so
  // the root descriptors are either not modified at all or already more
  // general than we requested. Take |root_nof| entries as is.
  // 0 -> |root_nof|
  int current_offset = 0;
  for (int i = 0; i < root_nof; ++i) {
    PropertyDetails old_details = old_descriptors_->GetDetails(i);
    if (old_details.location() == kField) {
      current_offset += old_details.field_width_in_words();
    }
    Descriptor d(handle(GetKey(i), isolate_),
                 handle(old_descriptors_->GetValue(i), isolate_), old_details);
    new_descriptors->Set(i, &d);
  }

  // Merge "updated" old_descriptor entries with target_descriptor entries.
  // |root_nof| -> |target_nof|
  for (int i = root_nof; i < target_nof; ++i) {
    Handle<Name> key(GetKey(i), isolate_);
    PropertyDetails old_details = GetDetails(i);
    PropertyDetails target_details = target_descriptors->GetDetails(i);

    PropertyKind next_kind = old_details.kind();
    PropertyAttributes next_attributes = old_details.attributes();
    PropertyLocation next_location =
        old_details.location() == kField ||
                target_details.location() == kField ||
                !EqualImmutableValues(target_descriptors->GetValue(i),
                                      GetValue(i))
            ? kField
            : kDescriptor;

    Representation next_representation =
        old_details.representation().generalize(
            target_details.representation());

    DCHECK_EQ(next_kind, target_details.kind());
    DCHECK_EQ(next_attributes, target_details.attributes());

    if (next_location == kField) {
      Handle<FieldType> old_field_type =
          GetOrComputeFieldType(i, old_details.location(), next_representation);

      Handle<FieldType> target_field_type =
          GetOrComputeFieldType(target_descriptors, i,
                                target_details.location(), next_representation);

      Handle<FieldType> next_field_type = Map::GeneralizeFieldType(
          old_details.representation(), old_field_type, next_representation,
          target_field_type, isolate_);

      Handle<Object> wrapped_type(Map::WrapFieldType(next_field_type));
      Descriptor d;
      if (next_kind == kData) {
        d = Descriptor::DataField(key, current_offset, wrapped_type,
                                  next_attributes, next_representation);
      } else {
        // TODO(ishell): mutable accessors are not implemented yet.
        UNIMPLEMENTED();
      }
      current_offset += d.GetDetails().field_width_in_words();
      new_descriptors->Set(i, &d);
    } else {
      DCHECK_EQ(kDescriptor, next_location);

      Handle<Object> value(GetValue(i), isolate_);
      Descriptor d;
      if (next_kind == kData) {
        d = Descriptor::DataConstant(key, value, next_attributes);
      } else {
        DCHECK_EQ(kAccessor, next_kind);
        d = Descriptor::AccessorConstant(key, value, next_attributes);
      }
      new_descriptors->Set(i, &d);
    }
  }

  // Take "updated" old_descriptor entries.
  // |target_nof| -> |old_nof|
  for (int i = target_nof; i < old_nof_; ++i) {
    PropertyDetails old_details = GetDetails(i);
    Handle<Name> key(GetKey(i), isolate_);

    PropertyKind next_kind = old_details.kind();
    PropertyAttributes next_attributes = old_details.attributes();
    PropertyLocation next_location = old_details.location();
    Representation next_representation = old_details.representation();

    Descriptor d;
    if (next_location == kField) {
      Handle<FieldType> old_field_type =
          GetOrComputeFieldType(i, old_details.location(), next_representation);

      Handle<Object> wrapped_type(Map::WrapFieldType(old_field_type));
      Descriptor d;
      if (next_kind == kData) {
        d = Descriptor::DataField(key, current_offset, wrapped_type,
                                  next_attributes, next_representation);
      } else {
        // TODO(ishell): mutable accessors are not implemented yet.
        UNIMPLEMENTED();
      }
      current_offset += d.GetDetails().field_width_in_words();
      new_descriptors->Set(i, &d);
    } else {
      DCHECK_EQ(kDescriptor, next_location);

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
  DisallowHeapAllocation no_allocation;

  int root_nof = root_map_->NumberOfOwnDescriptors();
  Map* current = *root_map_;
  for (int i = root_nof; i < old_nof_; i++) {
    Name* name = descriptors->GetKey(i);
    PropertyDetails details = descriptors->GetDetails(i);
    Map* next = TransitionArray::SearchTransition(current, details.kind(), name,
                                                  details.attributes());
    if (next == NULL) break;
    DescriptorArray* next_descriptors = next->instance_descriptors();

    PropertyDetails next_details = next_descriptors->GetDetails(i);
    DCHECK_EQ(details.kind(), next_details.kind());
    DCHECK_EQ(details.attributes(), next_details.attributes());
    if (details.location() != next_details.location()) break;
    if (!details.representation().Equals(next_details.representation())) break;

    if (next_details.location() == kField) {
      FieldType* next_field_type = next_descriptors->GetFieldType(i);
      if (!descriptors->GetFieldType(i)->NowIs(next_field_type)) {
        break;
      }
    } else {
      if (!EqualImmutableValues(descriptors->GetValue(i),
                                next_descriptors->GetValue(i))) {
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
  DCHECK_NE(old_nof_, split_nof);

  PropertyDetails split_details = GetDetails(split_nof);

  // Invalidate a transition target at |key|.
  Map* maybe_transition = TransitionArray::SearchTransition(
      *split_map, split_details.kind(), GetKey(split_nof),
      split_details.attributes());
  if (maybe_transition != NULL) {
    maybe_transition->DeprecateTransitionTree();
  }

  // If |maybe_transition| is not NULL then the transition array already
  // contains entry for given descriptor. This means that the transition
  // could be inserted regardless of whether transitions array is full or not.
  if (maybe_transition == NULL &&
      !TransitionArray::CanHaveMoreTransitions(split_map)) {
    return CopyGeneralizeAllFields("GenAll_CantHaveMoreTransitions");
  }

  old_map_->NotifyLeafMapLayoutChange();

  if (FLAG_trace_generalization && modified_descriptor_ >= 0) {
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
      old_value =
          handle(old_descriptors_->GetValue(modified_descriptor_), isolate_);
    }
    if (new_details.location() == kField) {
      new_field_type =
          handle(new_descriptors->GetFieldType(modified_descriptor_), isolate_);
    } else {
      new_value =
          handle(new_descriptors->GetValue(modified_descriptor_), isolate_);
    }

    old_map_->PrintGeneralization(
        stdout, "", modified_descriptor_, split_nof, old_nof_,
        old_details.location() == kDescriptor && new_location_ == kField,
        old_details.representation(), new_details.representation(),
        old_field_type, old_value, new_field_type, new_value);
  }

  Handle<LayoutDescriptor> new_layout_descriptor =
      LayoutDescriptor::New(split_map, new_descriptors, old_nof_);

  Handle<Map> new_map = Map::AddMissingTransitions(split_map, new_descriptors,
                                                   new_layout_descriptor);

  // Deprecated part of the transition tree is no longer reachable, so replace
  // current instance descriptors in the "survived" part of the tree with
  // the new descriptors to maintain descriptors sharing invariant.
  split_map->ReplaceDescriptors(*new_descriptors, *new_layout_descriptor);

  result_map_ = new_map;
  state_ = kEnd;
  return state_;  // Done.
}

}  // namespace internal
}  // namespace v8
