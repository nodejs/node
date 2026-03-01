// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_DATA_OBJECT_BUILDER_INL_H_
#define V8_OBJECTS_JS_DATA_OBJECT_BUILDER_INL_H_

#include "src/objects/js-data-object-builder.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/assert-scope.h"
#include "src/heap/factory.h"
#include "src/heap/heap-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/lookup.h"
#include "src/objects/map-updater.h"
#include "src/objects/objects-inl.h"
#include "src/objects/property-details.h"
#include "src/objects/transitions-inl.h"
#include "src/roots/roots-inl.h"
#include "src/utils/boxed-float.h"

namespace v8 {
namespace internal {

class FoldedMutableHeapNumberAllocation {
 public:
  // TODO(leszeks): If allocation alignment is ever enabled, we'll need to add
  // padding fillers between heap numbers.
  static_assert(!USE_ALLOCATION_ALIGNMENT_HEAP_NUMBER_BOOL);

  FoldedMutableHeapNumberAllocation(Isolate* isolate, int count) {
    if (count == 0) return;
    int size = count * sizeof(HeapNumber);
    raw_bytes_ = isolate->factory()->NewByteArray(size);
  }

  Handle<ByteArray> raw_bytes() const { return raw_bytes_; }

 private:
  Handle<ByteArray> raw_bytes_ = {};
};

class FoldedMutableHeapNumberAllocator {
 public:
  FoldedMutableHeapNumberAllocator(
      Isolate* isolate, FoldedMutableHeapNumberAllocation* allocation,
      DisallowGarbageCollection& no_gc)
      : isolate_(isolate), roots_(isolate) {
    if (allocation->raw_bytes().is_null()) return;

    raw_bytes_ = allocation->raw_bytes();
    mutable_double_address_ =
        reinterpret_cast<Address>(allocation->raw_bytes()->begin());
  }

  ~FoldedMutableHeapNumberAllocator() {
    // Make all mutable HeapNumbers alive.
    if (mutable_double_address_ == 0) {
      DCHECK(raw_bytes_.is_null());
      return;
    }

    DCHECK_EQ(mutable_double_address_,
              reinterpret_cast<Address>(raw_bytes_->end()));
    // Before setting the length of mutable_double_buffer back to zero, we
    // must ensure that the sweeper is not running or has already swept the
    // object's page. Otherwise the GC can add the contents of
    // mutable_double_buffer to the free list.
    isolate_->heap()->EnsureSweepingCompletedForObject(*raw_bytes_);
    raw_bytes_->set_length(0);
  }

  Tagged<HeapNumber> AllocateNext(ReadOnlyRoots roots, Float64 value) {
    DCHECK_GE(mutable_double_address_,
              reinterpret_cast<Address>(raw_bytes_->begin()));
    Tagged<HeapObject> hn = HeapObject::FromAddress(mutable_double_address_);
    hn->set_map_after_allocation(isolate_, roots.heap_number_map());
    Cast<HeapNumber>(hn)->set_value_as_bits(value.get_bits());
    mutable_double_address_ +=
        ALIGN_TO_ALLOCATION_ALIGNMENT(sizeof(HeapNumber));
    DCHECK_LE(mutable_double_address_,
              reinterpret_cast<Address>(raw_bytes_->end()));
    return Cast<HeapNumber>(hn);
  }

 private:
  Isolate* isolate_;
  ReadOnlyRoots roots_;
  Handle<ByteArray> raw_bytes_ = {};
  Address mutable_double_address_ = 0;
};

template <typename PropertyIterator>
Handle<JSObject> JSDataObjectBuilder::BuildFromIterator(
    PropertyIterator&& it, MaybeHandle<FixedArrayBase> maybe_elements) {
  Handle<String> failed_property_add_key;
  for (; !it.Done(); it.Advance()) {
    Handle<String> property_key;
    if (!TryAddFastPropertyForValue(
            it.GetKeyChars(),
            [&](Handle<String> expected_key) {
              return property_key = it.GetKey(expected_key);
            },
            [&]() { return it.GetValue(true); })) {
      failed_property_add_key = property_key;
      break;
    }
  }

  DirectHandle<FixedArrayBase> elements;
  if (!maybe_elements.ToHandle(&elements)) {
    elements = isolate_->factory()->empty_fixed_array();
  }
  CreateAndInitialiseObject(it.RevisitValues(), elements);

  // Slow path: define remaining named properties.
  for (; !it.Done(); it.Advance()) {
    DirectHandle<String> key;
    if (!failed_property_add_key.is_null()) {
      key = std::exchange(failed_property_add_key, {});
    } else {
      key = it.GetKey({});
    }
#ifdef DEBUG
    uint32_t index;
    DCHECK(!key->AsArrayIndex(&index));
#endif
    Handle<Object> value = it.GetValue(false);
    AddSlowProperty(key, value);
  }

  return object();
}

template <typename Char, typename GetKeyFunction, typename GetValueFunction>
bool JSDataObjectBuilder::TryAddFastPropertyForValue(
    base::Vector<const Char> key_chars, GetKeyFunction&& get_key,
    GetValueFunction&& get_value) {
  // The fast path is only valid as long as we haven't allocated an object
  // yet.
  DCHECK(object_.is_null());

  Handle<String> key;
  bool existing_map_found =
      TryFastTransitionToPropertyKey(key_chars, get_key, &key);
  // Unconditionally get the value after getting the transition result.
  DirectHandle<Object> value = get_value();
  if (existing_map_found) {
    // We found a map with a field for our value -- now make sure that field
    // is compatible with our value.
    if (!TryGeneralizeFieldToValue(value)) {
      // TODO(leszeks): Try to stay on the fast path if we just deprecate
      // here.
      return false;
    }
    AdvanceToNextProperty();
    return true;
  }

  // Try to stay on a semi-fast path (being able to stamp out the object
  // fields after creating the correct map) by manually creating the next
  // map here.

  Tagged<DescriptorArray> descriptors = map_->instance_descriptors(isolate_);
  InternalIndex descriptor_number =
      descriptors->SearchWithCache(isolate_, *key, *map_);
  if (descriptor_number.is_found()) {
    // Duplicate property, we need to bail out of even the semi-fast path
    // because we can no longer stamp out values linearly.
    return false;
  }

  if (!TransitionsAccessor::CanHaveMoreTransitions(isolate_, map_)) {
    return false;
  }

  Representation representation =
      Object::OptimalRepresentation(*value, isolate_);
  DirectHandle<FieldType> type =
      Object::OptimalType(*value, isolate_, representation);
  MaybeHandle<Map> maybe_map = Map::CopyWithField(
      isolate_, map_, key, type, NONE, PropertyConstness::kConst,
      representation, INSERT_TRANSITION);
  Handle<Map> next_map;
  if (!maybe_map.ToHandle(&next_map)) return false;
  if (next_map->is_dictionary_map()) return false;

  map_ = next_map;
  if (representation.IsDouble()) {
    RegisterFieldNeedsFreshHeapNumber(value);
  }
  AdvanceToNextProperty();
  return true;
}

template <typename ValueIterator>
void JSDataObjectBuilder::CreateAndInitialiseObject(
    ValueIterator value_it, DirectHandle<FixedArrayBase> elements) {
  // We've created a map for the first `i` property stack values (which might
  // be all of them). We need to write these properties to a newly allocated
  // object.
  DCHECK(object_.is_null());

  if (current_property_index_ < property_count_in_expected_final_map_) {
    // If we were on the expected map fast path all the way, but never reached
    // the expected final map itself, then finalize the map by rewinding to
    // the one whose property is the actual current property index.
    //
    // TODO(leszeks): Do we actually want to use the final map fast path when
    // we know that the current map _can't_ reach the final map? Will we even
    // hit this case given that we check for matching instance size?
    RewindExpectedFinalMapFastPathToBeforeCurrent();
  }

  if (map_->is_dictionary_map()) {
    // It's only safe to emit a dictionary map when we've not set up any
    // properties, as the caller assumes it can set up the first N properties
    // as fast data properties.
    DCHECK_EQ(current_property_index_, 0);

    Handle<JSObject> object = isolate_->factory()->NewSlowJSObjectFromMap(
        map_, expected_property_count_);
    object->set_elements(*elements);
    object_ = object;
    return;
  }

  // The map should have as many own descriptors as the number of properties
  // we've created so far...
  DCHECK_EQ(current_property_index_, map_->NumberOfOwnDescriptors());

  // ... and all of those properties should be in-object data properties.
  DCHECK_EQ(current_property_index_,
            map_->GetInObjectProperties() - map_->UnusedInObjectProperties());

  // Create a folded mutable HeapNumber allocation area before allocating the
  // object -- this ensures that there is no allocation between the object
  // allocation and its initial fields being initialised, where the verifier
  // would see invalid double field state.
  FoldedMutableHeapNumberAllocation hn_allocation(isolate_,
                                                  extra_heap_numbers_needed_);

  // Allocate the object then immediately start a no_gc scope -- again, this
  // is so the verifier doesn't see invalid double field state.
  Handle<JSObject> object = isolate_->factory()->NewJSObjectFromMap(
      map_, AllocationType::kYoung, DirectHandle<AllocationSite>::null(),
      NewJSObjectType::kNoEmbedderFieldsAndNoApiWrapper);
  DisallowGarbageCollection no_gc;
  Tagged<JSObject> raw_object = *object;

  raw_object->set_elements(*elements);
  Tagged<DescriptorArray> descriptors =
      raw_object->map()->instance_descriptors();

  FoldedMutableHeapNumberAllocator hn_allocator(isolate_, &hn_allocation,
                                                no_gc);

  ReadOnlyRoots roots(isolate_);

  // Initialize the in-object properties up to the last added property.
  int current_property_offset = raw_object->GetInObjectPropertyOffset(0);
  for (int i = 0; i < current_property_index_; ++i, ++value_it) {
    InternalIndex descriptor_index(i);
    Tagged<Object> value = **value_it;

    // See comment in RegisterFieldNeedsFreshHeapNumber, we need to allocate
    // HeapNumbers for double representation fields when we can't make
    // existing HeapNumbers mutable, or when we only have a Smi value.
    if (heap_number_mode_ != kHeapNumbersGuaranteedUniquelyOwned ||
        IsSmi(value)) {
      PropertyDetails details = descriptors->GetDetails(descriptor_index);
      if (details.representation().IsDouble()) {
        value = hn_allocator.AllocateNext(roots,
                                          Float64(Object::NumberValue(value)));
      }
    }

    DCHECK(FieldIndex::ForPropertyIndex(object->map(), i).is_inobject());
    DCHECK_EQ(current_property_offset,
              FieldIndex::ForPropertyIndex(object->map(), i).offset());
    DCHECK_EQ(current_property_offset,
              object->map()->GetInObjectPropertyOffset(i));
    FieldIndex index = FieldIndex::ForInObjectOffset(current_property_offset,
                                                     FieldIndex::kTagged);
    // Object is the most recent young allocation, so no write barrier
    // required.
    raw_object->RawFastInobjectPropertyAtPut(index, value, SKIP_WRITE_BARRIER);
    current_property_offset += kTaggedSize;
  }
  DCHECK_EQ(current_property_offset,
            object->map()->GetInObjectPropertyOffset(current_property_index_));

  object_ = object;
}

template <typename Char, typename GetKeyFunction>
bool JSDataObjectBuilder::TryFastTransitionToPropertyKey(
    base::Vector<const Char> key_chars, GetKeyFunction&& get_key,
    Handle<String>* key_out) {
  Handle<String> expected_key;
  DirectHandle<Map> target_map;

  InternalIndex descriptor_index(current_property_index_);
  if (IsOnExpectedFinalMapFastPath()) {
    expected_key =
        handle(Cast<String>(
                   expected_final_map_->instance_descriptors(isolate_)->GetKey(
                       descriptor_index)),
               isolate_);
    target_map = expected_final_map_;
  } else {
    TransitionsAccessor transitions(isolate_, *map_);
    auto expected_transition = transitions.ExpectedTransition(key_chars);
    if (!expected_transition.first.is_null()) {
      // Directly read out the target while reading out the key, otherwise it
      // might die if `get_key` can allocate.
      target_map = expected_transition.second;

      // We were successful and we are done.
      DCHECK_EQ(target_map->instance_descriptors()
                    ->GetDetails(descriptor_index)
                    .location(),
                PropertyLocation::kField);
      map_ = target_map;
      return true;
    }
  }

  DirectHandle<String> key = *key_out = get_key(expected_key);
  if (key.is_identical_to(expected_key)) {
    // We were successful and we are done.
    DCHECK_EQ(target_map->instance_descriptors()
                  ->GetDetails(descriptor_index)
                  .location(),
              PropertyLocation::kField);
    map_ = target_map;
    return true;
  }

  if (IsOnExpectedFinalMapFastPath()) {
    // We were on the expected map fast path, but this missed that fast
    // path, so rewind the optimistic setting of the current map and disable
    // this fast path.
    RewindExpectedFinalMapFastPathToBeforeCurrent();
    property_count_in_expected_final_map_ = 0;
  }

  MaybeHandle<Map> maybe_target =
      TransitionsAccessor(isolate_, *map_).FindTransitionToField(key);
  if (!maybe_target.ToHandle(&target_map)) return false;

  map_ = target_map;
  return true;
}

bool JSDataObjectBuilder::TryGeneralizeFieldToValue(
    DirectHandle<Object> value) {
  DCHECK_LT(current_property_index_, map_->NumberOfOwnDescriptors());

  InternalIndex descriptor_index(current_property_index_);
  PropertyDetails current_details =
      map_->instance_descriptors(isolate_)->GetDetails(descriptor_index);
  Representation expected_representation = current_details.representation();

  DCHECK_EQ(current_details.kind(), PropertyKind::kData);
  DCHECK_EQ(current_details.location(), PropertyLocation::kField);

  if (!Object::FitsRepresentation(*value, expected_representation)) {
    Representation representation =
        Object::OptimalRepresentation(*value, isolate_);
    representation = representation.generalize(expected_representation);
    if (!expected_representation.CanBeInPlaceChangedTo(representation)) {
      // Reconfigure the map for the value, deprecating if necessary. This
      // will only happen for double representation fields.
      if (IsOnExpectedFinalMapFastPath()) {
        // If we're on the fast path, we will have advanced the current map
        // all the way to the final expected map. Make sure to rewind to the
        // "real" current map if this happened.
        //
        // An alternative would be to deprecate the expected final map,
        // migrate it to the new representation, and stay on the fast path.
        // However, this would mean allocating all-new maps (with the new
        // representation) all the way between the current map and the new
        // expected final map; if we later fall off the fast path anyway, then
        // all those newly allocated maps will end up unused.
        RewindExpectedFinalMapFastPathToIncludeCurrent();
        property_count_in_expected_final_map_ = 0;
      }
      MapUpdater mu(isolate_, map_);
      Handle<Map> new_map = mu.ReconfigureToDataField(
          descriptor_index, current_details.attributes(),
          current_details.constness(), representation,
          FieldType::Any(isolate_));

      // We only want to stay on the fast path if we got a fast map.
      if (new_map->is_dictionary_map()) return false;
      map_ = new_map;
      DCHECK(representation.IsDouble());
      RegisterFieldNeedsFreshHeapNumber(value);
    } else {
      // Do the in-place reconfiguration.
      DCHECK(!representation.IsDouble());
      DirectHandle<FieldType> value_type =
          Object::OptimalType(*value, isolate_, representation);
      MapUpdater::GeneralizeField(isolate_, map_, descriptor_index,
                                  current_details.constness(), representation,
                                  value_type);
    }
  } else if (expected_representation.IsHeapObject() &&
             !FieldType::NowContains(
                 map_->instance_descriptors(isolate_)->GetFieldType(
                     descriptor_index),
                 value)) {
    DirectHandle<FieldType> value_type =
        Object::OptimalType(*value, isolate_, expected_representation);
    MapUpdater::GeneralizeField(isolate_, map_, descriptor_index,
                                current_details.constness(),
                                expected_representation, value_type);
  } else if (expected_representation.IsDouble()) {
    RegisterFieldNeedsFreshHeapNumber(value);
  }

  DCHECK(FieldType::NowContains(
      map_->instance_descriptors(isolate_)->GetFieldType(descriptor_index),
      value));
  return true;
}

void JSDataObjectBuilder::RegisterFieldNeedsFreshHeapNumber(
    DirectHandle<Object> value) {
  // We need to allocate a new HeapNumber for double representation fields if
  // the HeapNumber values is not guaranteed to be uniquely owned by this
  // object (and therefore can't be made mutable), or if the value is a Smi
  // and there is no HeapNumber box for this value yet at all.
  if (heap_number_mode_ == kHeapNumbersGuaranteedUniquelyOwned &&
      !IsSmi(*value)) {
    DCHECK(IsHeapNumber(*value));
    return;
  }
  extra_heap_numbers_needed_++;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_JS_DATA_OBJECT_BUILDER_INL_H_
