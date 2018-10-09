// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAP_UPDATER_H_
#define V8_MAP_UPDATER_H_

#include "src/elements-kind.h"
#include "src/field-type.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/objects/map.h"
#include "src/property-details.h"

namespace v8 {
namespace internal {

// The |MapUpdater| class implements all sorts of map reconfigurations
// including changes of elements kind, property attributes, property kind,
// property location and field representations/type changes. It ensures that
// the reconfigured map and all the intermediate maps are properly integrated
// into the exising transition tree.
//
// To avoid high degrees over polymorphism, and to stabilize quickly, on every
// rewrite the new type is deduced by merging the current type with any
// potential new (partial) version of the type in the transition tree.
// To do this, on each rewrite:
// - Search the root of the transition tree using FindRootMap.
// - Find/create a |root_map| with requested |new_elements_kind|.
// - Find |target_map|, the newest matching version of this map using the
//   "updated" |old_map|'s descriptor array (i.e. whose entry at |modify_index|
//   is considered to be of |new_kind| and having |new_attributes|) to walk
//   the transition tree.
// - Merge/generalize the "updated" descriptor array of the |old_map| and
//   descriptor array of the |target_map|.
// - Generalize the |modify_index| descriptor using |new_representation| and
//   |new_field_type|.
// - Walk the tree again starting from the root towards |target_map|. Stop at
//   |split_map|, the first map who's descriptor array does not match the merged
//   descriptor array.
// - If |target_map| == |split_map|, |target_map| is in the expected state.
//   Return it.
// - Otherwise, invalidate the outdated transition target from |target_map|, and
//   replace its transition tree with a new branch for the updated descriptors.
class MapUpdater {
 public:
  MapUpdater(Isolate* isolate, Handle<Map> old_map);

  // Prepares for reconfiguring of a property at |descriptor| to data field
  // with given |attributes| and |representation|/|field_type| and
  // performs the steps 1-5.
  Handle<Map> ReconfigureToDataField(int descriptor,
                                     PropertyAttributes attributes,
                                     PropertyConstness constness,
                                     Representation representation,
                                     Handle<FieldType> field_type);

  // Prepares for reconfiguring elements kind and performs the steps 1-5.
  Handle<Map> ReconfigureElementsKind(ElementsKind elements_kind);

  // Prepares for updating deprecated map to most up-to-date non-deprecated
  // version and performs the steps 1-5.
  Handle<Map> Update();

 private:
  enum State { kInitialized, kAtRootMap, kAtTargetMap, kEnd };

  // Try to reconfigure property in-place without rebuilding transition tree
  // and creating new maps. See implementation for details.
  State TryRecofigureToDataFieldInplace();

  // Step 1.
  // - Search the root of the transition tree using FindRootMap.
  // - Find/create a |root_map_| with requested |new_elements_kind_|.
  State FindRootMap();

  // Step 2.
  // - Find |target_map_|, the newest matching version of this map using the
  //   "updated" |old_map|'s descriptor array (i.e. whose entry at
  //   |modified_descriptor_| is considered to be of |new_kind| and having
  //   |new_attributes|) to walk the transition tree.
  State FindTargetMap();

  // Step 3.
  // - Merge/generalize the "updated" descriptor array of the |old_map_| and
  //   descriptor array of the |target_map_|.
  // - Generalize the |modified_descriptor_| using |new_representation| and
  //   |new_field_type_|.
  Handle<DescriptorArray> BuildDescriptorArray();

  // Step 4.
  // - Walk the tree again starting from the root towards |target_map|. Stop at
  //   |split_map|, the first map who's descriptor array does not match the
  //   merged descriptor array.
  Handle<Map> FindSplitMap(Handle<DescriptorArray> descriptors);

  // Step 5.
  // - If |target_map| == |split_map|, |target_map| is in the expected state.
  //   Return it.
  // - Otherwise, invalidate the outdated transition target from |target_map|,
  //   and replace its transition tree with a new branch for the updated
  //   descriptors.
  State ConstructNewMap();

  // When a requested reconfiguration can not be done the result is a copy
  // of |old_map_| where every field has |Tagged| representation and |Any|
  // field type. This map is disconnected from the transition tree.
  State CopyGeneralizeAllFields(const char* reason);

  // Returns name of a |descriptor| property.
  inline Name* GetKey(int descriptor) const;

  // Returns property details of a |descriptor| in "updated" |old_descrtiptors_|
  // array.
  inline PropertyDetails GetDetails(int descriptor) const;

  // Returns value of a |descriptor| with kDescriptor location in "updated"
  // |old_descrtiptors_| array.
  inline Object* GetValue(int descriptor) const;

  // Returns field type for a |descriptor| with kField location in "updated"
  // |old_descrtiptors_| array.
  inline FieldType* GetFieldType(int descriptor) const;

  // If a |descriptor| property in "updated" |old_descriptors_| has kField
  // location then returns it's field type otherwise computes optimal field
  // type for the descriptor's value and |representation|. The |location|
  // value must be a pre-fetched location for |descriptor|.
  inline Handle<FieldType> GetOrComputeFieldType(
      int descriptor, PropertyLocation location,
      Representation representation) const;

  // If a |descriptor| property in given |descriptors| array has kField
  // location then returns it's field type otherwise computes optimal field
  // type for the descriptor's value and |representation|.
  // The |location| value must be a pre-fetched location for |descriptor|.
  inline Handle<FieldType> GetOrComputeFieldType(
      Handle<DescriptorArray> descriptors, int descriptor,
      PropertyLocation location, Representation representation);

  void GeneralizeField(Handle<Map> map, int modify_index,
                       PropertyConstness new_constness,
                       Representation new_representation,
                       Handle<FieldType> new_field_type);

  Isolate* isolate_;
  Handle<Map> old_map_;
  Handle<DescriptorArray> old_descriptors_;
  Handle<Map> root_map_;
  Handle<Map> target_map_;
  Handle<Map> result_map_;
  int old_nof_;

  State state_ = kInitialized;
  ElementsKind new_elements_kind_;
  bool is_transitionable_fast_elements_kind_;

  // If |modified_descriptor_| is not equal to -1 then the fields below form
  // an "update" of the |old_map_|'s descriptors.
  int modified_descriptor_ = -1;
  PropertyKind new_kind_ = kData;
  PropertyAttributes new_attributes_ = NONE;
  PropertyConstness new_constness_ = PropertyConstness::kMutable;
  PropertyLocation new_location_ = kField;
  Representation new_representation_ = Representation::None();

  // Data specific to kField location.
  Handle<FieldType> new_field_type_;

  // Data specific to kDescriptor location.
  Handle<Object> new_value_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_MAP_UPDATER_H_
