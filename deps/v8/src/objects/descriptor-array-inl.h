// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DESCRIPTOR_ARRAY_INL_H_
#define V8_OBJECTS_DESCRIPTOR_ARRAY_INL_H_

#include "src/objects/descriptor-array.h"
// Include the non-inl header before the rest of the headers.

#include "src/execution/isolate.h"
#include "src/handles/maybe-handles-inl.h"
#include "src/heap/heap-write-barrier.h"
#include "src/heap/heap.h"
#include "src/objects/api-callbacks.h"
#include "src/objects/bigint.h"
#include "src/objects/dictionary.h"
#include "src/objects/field-type.h"
#include "src/objects/heap-number.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/lookup-cache-inl.h"
#include "src/objects/lookup-table-inl.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/property-details-inl.h"
#include "src/objects/property.h"
#include "src/objects/struct-inl.h"
#include "src/objects/tagged-field-inl.h"
#include "src/torque/runtime-macro-shims.h"
#include "src/torque/runtime-support.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

EnumCache::EnumCache(const AllocationWitness& witness, Tagged<FixedArray> keys,
                     Tagged<FixedArray> indices)
    : Struct(witness.roots().enum_cache_map()),
      keys_(witness, keys),
      indices_(witness, indices) {}

Tagged<FixedArray> EnumCache::keys() const { return keys_.load(); }
void EnumCache::set_keys(Tagged<FixedArray> value, WriteBarrierMode mode) {
  keys_.store(this, value, mode);
}

Tagged<FixedArray> EnumCache::indices() const { return indices_.load(); }
void EnumCache::set_indices(Tagged<FixedArray> value, WriteBarrierMode mode) {
  indices_.store(this, value, mode);
}

template <typename IsolateT>
Handle<DescriptorLookupTable> DescriptorLookupTable::New(
    IsolateT* isolate, uint32_t number_of_elements, AllocationType allocation) {
  return Cast<DescriptorLookupTable>(
      Super::New(isolate, number_of_elements, allocation));
}

int16_t DescriptorArray::number_of_all_descriptors() const {
  return static_cast<int16_t>(
      number_of_all_descriptors_.load(std::memory_order_acquire));
}

void DescriptorArray::set_number_of_all_descriptors(int16_t value,
                                                    ReleaseStoreTag) {
  number_of_all_descriptors_.store(static_cast<uint16_t>(value),
                                   std::memory_order_release);
}

int16_t DescriptorArray::number_of_descriptors() const {
  return static_cast<int16_t>(
      number_of_descriptors_.load(std::memory_order_relaxed));
}

void DescriptorArray::set_number_of_descriptors(int16_t value) {
  number_of_descriptors_.store(static_cast<uint16_t>(value),
                               std::memory_order_relaxed);
}

DescriptorArray::Flags DescriptorArray::flags(RelaxedLoadTag) const {
  return flags_.load(std::memory_order_relaxed);
}

void DescriptorArray::set_flags(Flags value, RelaxedStoreTag) {
  flags_.store(value, std::memory_order_relaxed);
}

inline int16_t DescriptorArray::number_of_slack_descriptors() const {
  return number_of_all_descriptors() - number_of_descriptors();
}

inline int DescriptorArray::number_of_entries() const {
  return number_of_descriptors();
}

Tagged<EnumCache> DescriptorArray::enum_cache() const {
  return enum_cache_.load();
}

void DescriptorArray::set_enum_cache(Tagged<EnumCache> value,
                                     WriteBarrierMode mode) {
  enum_cache_.store(this, value, mode);
}

#ifdef V8_ENABLE_DESCRIPTOR_ARRAY_LOOKUP_TABLE
Tagged<UnionOf<DescriptorLookupTable, Undefined>>
DescriptorArray::lookup_table() const {
  return lookup_table_.load();
}

void DescriptorArray::set_lookup_table(
    Tagged<UnionOf<DescriptorLookupTable, Undefined>> value,
    WriteBarrierMode mode) {
  lookup_table_.store(this, value, mode);
}

void DescriptorArray::InsertIntoLookupTable(Tagged<Name> key, uint32_t hash,
                                            int descriptor_index) {
  DCHECK(!IsUndefined(lookup_table()));
  Cast<DescriptorLookupTable>(lookup_table())
      ->Insert(key, hash, descriptor_index,
               [this](uint32_t index) { return GetKey(InternalIndex(index)); });
}

void DescriptorArray::RebuildLookupTable(int len) {
  DCHECK_LE(len, number_of_descriptors());
  DCHECK_GE(len, kMaxElementsForLinearSearch);
  DCHECK(!IsUndefined(lookup_table()));
  Cast<DescriptorLookupTable>(lookup_table())
      ->Rebuild(
          len, [this](uint32_t index) { return GetKey(InternalIndex(index)); });
}

void DescriptorArray::CopyFromOrRebuildLookupTable(
    Tagged<DescriptorArray> source) {
  int new_nof = number_of_descriptors();
  if (new_nof <= kMaxElementsForLinearSearch) return;
  int old_nof = source->number_of_descriptors();
  DCHECK_LE(new_nof, old_nof);
  Tagged<DescriptorLookupTable> source_table =
      Cast<DescriptorLookupTable>(source->lookup_table());
  Tagged<DescriptorLookupTable> table =
      Cast<DescriptorLookupTable>(lookup_table());
  if (table->capacity() == source_table->capacity() &&
      old_nof - new_nof <= new_nof) {
    table->CopyFrom(source_table);
    table->RemoveEntries(old_nof, new_nof, [source](uint32_t index) {
      return source->GetKey(InternalIndex(index));
    });
  } else {
    RebuildLookupTable(new_nof);
  }
  DCHECK(VerifyLookupTable());
}

void DescriptorArray::RemoveFromLookupTable(int old_nof, int new_nof) {
  DCHECK_EQ(old_nof, number_of_descriptors());
  DCHECK_GT(new_nof, kMaxElementsForLinearSearch);
  DCHECK(!IsUndefined(lookup_table()));
  Cast<DescriptorLookupTable>(lookup_table())
      ->RemoveEntries(old_nof, new_nof, [this](uint32_t index) {
        return GetKey(InternalIndex(index));
      });
}
#endif  // V8_ENABLE_DESCRIPTOR_ARRAY_LOOKUP_TABLE

DescriptorArray::FastIterableState DescriptorArray::fast_iterable() const {
  return flags(kRelaxedLoad).fast_iterable();
}

void DescriptorArray::set_fast_iterable(FastIterableState value) {
  set_flags(flags(kRelaxedLoad).with_fast_iterable(value), kRelaxedStore);
}

void DescriptorArray::set_fast_iterable_if(FastIterableState new_value,
                                           FastIterableState if_value) {
  Flags f = flags(kRelaxedLoad);
  if (f.fast_iterable() == if_value) {
    set_flags(f.with_fast_iterable(new_value), kRelaxedStore);
  }
}

void DescriptorArray::CopyEnumCacheFrom(Tagged<DescriptorArray> array) {
  set_enum_cache(array->enum_cache());
}

InternalIndex DescriptorArray::Search(Tagged<Name> name, int valid_descriptors,
                                      bool concurrent_search) {
  DCHECK(IsUniqueName(name));
#ifdef V8_ENABLE_DESCRIPTOR_ARRAY_LOOKUP_TABLE
  SLOW_DCHECK_IMPLIES(!concurrent_search, VerifyLookupTable());
#else
  SLOW_DCHECK_IMPLIES(!concurrent_search, IsSortedNoDuplicates());
#endif  // V8_ENABLE_DESCRIPTOR_ARRAY_LOOKUP_TABLE

  if (valid_descriptors == 0) {
    return InternalIndex::NotFound();
  }

  if (valid_descriptors <= kMaxElementsForLinearSearch || concurrent_search) {
    return LinearSearch(name, valid_descriptors);
  }

#ifdef V8_ENABLE_DESCRIPTOR_ARRAY_LOOKUP_TABLE
  return TableSearch(name, valid_descriptors);
#else
  return BinarySearch(name, valid_descriptors);
#endif  // V8_ENABLE_DESCRIPTOR_ARRAY_LOOKUP_TABLE
}

#ifdef V8_ENABLE_DESCRIPTOR_ARRAY_LOOKUP_TABLE
InternalIndex DescriptorArray::TableSearch(Tagged<Name> name,
                                           int valid_descriptors) {
  DCHECK_LT(kMaxElementsForLinearSearch, number_of_descriptors());
  uint32_t index;
  if (Cast<DescriptorLookupTable>(lookup_table())
          ->Lookup(
              name, valid_descriptors,
              [this](uint32_t i) { return GetKey(InternalIndex(i)); },
              &index)) {
    return InternalIndex(index);
  }
  return InternalIndex::NotFound();
}
#endif  // V8_ENABLE_DESCRIPTOR_ARRAY_LOOKUP_TABLE

InternalIndex DescriptorArray::BinarySearch(Tagged<Name> name,
                                            int valid_descriptors) {
  // We have to binary search all descriptors, not just valid ones, since the
  // binary search ordering is across all descriptors.
  int end = number_of_descriptors();
  // Binary search must not be used for small number of descriptors since
  // the descriptor array is not sorted yet.
  DCHECK_LT(kMaxElementsForLinearSearch, end);
  uint32_t hash = name->hash();

  // Find the first descriptor whose key's hash is greater-than-or-equal-to the
  // search hash.
  int low = 0;
  int high = end;
  while (low < high) {
    int mid = low + (high - low) / 2;
    if (GetSortedKey(mid)->hash() < hash) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  int number = low;

  // There may have been hash collisions, so search for the name from the first
  // index until the first non-matching hash.
  for (; number < end; ++number) {
    InternalIndex index(GetSortedKeyIndex(number));
    Tagged<Name> entry = GetKey(index);
    if (entry == name) {
      // If we found the entry, but it's outside the owned descriptors of the
      // caller, return not found.
      if (index.as_int() >= valid_descriptors) {
        return InternalIndex::NotFound();
      }
      return index;
    }
    if (entry->hash() != hash) {
      return InternalIndex::NotFound();
    }
  }

  return InternalIndex::NotFound();
}

InternalIndex DescriptorArray::LinearSearch(Tagged<Name> name,
                                            int valid_descriptors) {
  DCHECK_LE(valid_descriptors, number_of_descriptors());
  for (int i = 0; i < valid_descriptors; ++i) {
    InternalIndex index(i);
    if (GetKey(index) == name) return index;
  }
  return InternalIndex::NotFound();
}

InternalIndex DescriptorArray::Search(Tagged<Name> name, Tagged<Map> map,
                                      bool concurrent_search) {
  DCHECK(IsUniqueName(name));
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  if (number_of_own_descriptors == 0) return InternalIndex::NotFound();
  return Search(name, number_of_own_descriptors, concurrent_search);
}

InternalIndex DescriptorArray::SearchWithCache(Isolate* isolate,
                                               Tagged<Name> name,
                                               Tagged<Map> map) {
  DCHECK(IsUniqueName(name));
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  if (number_of_own_descriptors == 0) return InternalIndex::NotFound();

  DescriptorLookupCache* cache = isolate->descriptor_lookup_cache();
  int number = cache->Lookup(map, name);

  if (number == DescriptorLookupCache::kAbsent) {
    InternalIndex result = Search(name, number_of_own_descriptors);
    number = result.is_found() ? result.as_int() : DescriptorArray::kNotFound;
    cache->Update(map, name, number);
  }
  if (number == DescriptorArray::kNotFound) return InternalIndex::NotFound();
  return InternalIndex(number);
}

ObjectSlot DescriptorArray::GetFirstPointerSlot() {
  return ObjectSlot(reinterpret_cast<Address>(&enum_cache_));
}

ObjectSlot DescriptorArray::GetDescriptorSlot(int descriptor) {
  // Allow descriptor == number_of_all_descriptors() for computing the slot
  // address that comes after the last descriptor (for iterating).
  DCHECK_LE(descriptor, number_of_all_descriptors());
  return ObjectSlot(reinterpret_cast<Address>(&entries()[descriptor]));
}

bool DescriptorArray::IsInitializedDescriptor(
    InternalIndex descriptor_number) const {
  DCHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  const Entry& entry = entries()[descriptor_number.as_int()];
  bool is_initialized = !IsUndefined(entry.key.Relaxed_Load());
  DCHECK_IMPLIES(is_initialized, IsSmi(entry.details.Relaxed_Load()));
  return is_initialized;
}

Tagged<Name> DescriptorArray::GetKey(InternalIndex descriptor_number) const {
  DCHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  return Cast<Name>(entries()[descriptor_number.as_int()].key.Relaxed_Load());
}

void DescriptorArray::SetKey(InternalIndex descriptor_number,
                             Tagged<Name> key) {
  DCHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  entries()[descriptor_number.as_int()].key.Relaxed_Store(this, key);
  // Conservatively assume that the new key might break fast iteration.
  // If the key was already known to be slow, it will stay slow.
  set_fast_iterable_if(FastIterableState::kUnknown,
                       FastIterableState::kJsonFast);
}

int DescriptorArray::GetSortedKeyIndex(int descriptor_number) {
  return GetDetails(InternalIndex(descriptor_number)).pointer();
}

Tagged<Name> DescriptorArray::GetSortedKey(int descriptor_number) {
  return GetKey(InternalIndex(GetSortedKeyIndex(descriptor_number)));
}

void DescriptorArray::SetSortedKey(int descriptor_number, int pointer) {
  PropertyDetails details = GetDetails(InternalIndex(descriptor_number));
  SetDetails(InternalIndex(descriptor_number), details.set_pointer(pointer));
}

Tagged<Object> DescriptorArray::GetStrongValue(
    InternalIndex descriptor_number) {
  return Cast<Object>(GetValue(descriptor_number));
}

void DescriptorArray::SetValue(InternalIndex descriptor_number,
                               Tagged<MaybeObject> value) {
  DCHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  using ValueT = UnionOf<JSAny, Weak<Map>, AccessorInfo, AccessorPair,
                         ClassPositions, NumberDictionary>;
  entries()[descriptor_number.as_int()].value.Relaxed_Store(
      this, UncheckedCast<ValueT>(value));
}

Tagged<MaybeObject> DescriptorArray::GetValue(InternalIndex descriptor_number) {
  DCHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  return entries()[descriptor_number.as_int()].value.Relaxed_Load();
}

PropertyDetails DescriptorArray::GetDetails(InternalIndex descriptor_number) {
  DCHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  return PropertyDetails(
      Cast<Smi>(entries()[descriptor_number.as_int()].details.Relaxed_Load()));
}

void DescriptorArray::SetDetails(InternalIndex descriptor_number,
                                 PropertyDetails details) {
  DCHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  entries()[descriptor_number.as_int()].details.Relaxed_Store(this,
                                                              details.AsSmi());
  // Note: fast_iteration depends on PropertyDetails::location().
  // However we don't reset it here as all path either go through SetKey(),
  // which invalidates fast_iteration, or don't change the location
  // (GeneralizeAllFields()).
}

int DescriptorArray::GetOffsetInWords(InternalIndex descriptor_number) {
  DCHECK_EQ(GetDetails(descriptor_number).location(), PropertyLocation::kField);
  return GetDetails(descriptor_number).field_offset();
}

Tagged<FieldType> DescriptorArray::GetFieldType(
    InternalIndex descriptor_number) {
  DCHECK_EQ(GetDetails(descriptor_number).location(), PropertyLocation::kField);
  Tagged<MaybeObject> wrapped_type = GetValue(descriptor_number);
  return Map::UnwrapFieldType(wrapped_type);
}

void DescriptorArray::Set(InternalIndex descriptor_number, Tagged<Name> key,
                          Tagged<MaybeObject> value, PropertyDetails details) {
  CHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  SetKey(descriptor_number, key);
  SetDetails(descriptor_number, details);
  SetValue(descriptor_number, value);
  // Resetting the fast iterable state is bottlenecked in SetKey().
  DCHECK_NE(fast_iterable(), FastIterableState::kJsonFast);
}

void DescriptorArray::Set(InternalIndex descriptor_number, Descriptor* desc) {
  Tagged<Name> key = *desc->GetKey();
  Tagged<MaybeObject> value = *desc->GetValue();
  Set(descriptor_number, key, value, desc->GetDetails());
  // Resetting the fast iterable state is bottlenecked in SetKey().
  DCHECK_NE(fast_iterable(), FastIterableState::kJsonFast);
}

void DescriptorArray::CopyFrom(InternalIndex index,
                               Tagged<DescriptorArray> src) {
  PropertyDetails details = src->GetDetails(index);
  Set(index, src->GetKey(index), src->GetValue(index), details);
}

void DescriptorArray::Append(Descriptor* desc) {
  DisallowGarbageCollection no_gc;
  int descriptor_number = number_of_descriptors();
  int new_number_of_descriptors = descriptor_number + 1;
  DCHECK_LE(new_number_of_descriptors, number_of_all_descriptors());
  set_number_of_descriptors(new_number_of_descriptors);
  Set(InternalIndex(descriptor_number), desc);

  // Resetting the fast iterable state is bottlenecked in SetKey().
  DCHECK_NE(fast_iterable(), FastIterableState::kJsonFast);

  if (new_number_of_descriptors <= kMaxElementsForLinearSearch) {
    // Ensure there are no name collisions.
    CHECK_EQ(LinearSearch(*desc->GetKey(), descriptor_number),
             InternalIndex::NotFound());
    return;
  }

  uint32_t desc_hash = desc->GetKey()->hash();
#ifdef V8_ENABLE_DESCRIPTOR_ARRAY_LOOKUP_TABLE
  InsertIntoLookupTable(*desc->GetKey(), desc_hash, descriptor_number);
#else
  if (new_number_of_descriptors == kMaxElementsForLinearSearch + 1) {
    // Sort descriptors as we've just crossed the unsorted-sorted boundary.
    SortImpl(new_number_of_descriptors);

    CheckNameCollisionDuringInsertion(desc, desc_hash,
                                      desc->GetSortedKeyIndex());
    return;
  }

  // Hash value can't be zero, see String::ComputeAndSetHash()
  uint32_t collision_hash = 0;

  int insertion;
  for (insertion = descriptor_number; insertion > 0; --insertion) {
    Tagged<Name> key = GetSortedKey(insertion - 1);
    collision_hash = key->hash();
    if (collision_hash <= desc_hash) break;
    SetSortedKey(insertion, GetSortedKeyIndex(insertion - 1));
  }

  SetSortedKey(insertion, descriptor_number);

  if (V8_LIKELY(collision_hash != desc_hash)) return;

  CheckNameCollisionDuringInsertion(desc, desc_hash, insertion);
#endif  // V8_ENABLE_DESCRIPTOR_ARRAY_LOOKUP_TABLE
}

void DescriptorArray::Sort() {
  const int len = number_of_descriptors();
  // Sorting matters only for binary search.
  if (len <= kMaxElementsForLinearSearch) return;
  SortImpl(len);
}

void DescriptorArray::SwapSortedKeys(int first, int second) {
  int first_key = GetSortedKeyIndex(first);
  SetSortedKey(first, GetSortedKeyIndex(second));
  SetSortedKey(second, first_key);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DESCRIPTOR_ARRAY_INL_H_
