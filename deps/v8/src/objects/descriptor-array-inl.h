// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DESCRIPTOR_ARRAY_INL_H_
#define V8_OBJECTS_DESCRIPTOR_ARRAY_INL_H_

#include "src/objects/descriptor-array.h"

#include "src/field-type.h"
#include "src/heap/heap-write-barrier.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/lookup-cache.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/struct-inl.h"
#include "src/property.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(DescriptorArray, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(EnumCache, Tuple2)

CAST_ACCESSOR(DescriptorArray)
CAST_ACCESSOR(EnumCache)

ACCESSORS(DescriptorArray, enum_cache, EnumCache, kEnumCacheOffset)
RELAXED_INT16_ACCESSORS(DescriptorArray, number_of_all_descriptors,
                        kNumberOfAllDescriptorsOffset)
RELAXED_INT16_ACCESSORS(DescriptorArray, number_of_descriptors,
                        kNumberOfDescriptorsOffset)
RELAXED_INT16_ACCESSORS(DescriptorArray, raw_number_of_marked_descriptors,
                        kRawNumberOfMarkedDescriptorsOffset)
RELAXED_INT16_ACCESSORS(DescriptorArray, filler16bits, kFiller16BitsOffset)

inline int16_t DescriptorArray::number_of_slack_descriptors() const {
  return number_of_all_descriptors() - number_of_descriptors();
}

inline int DescriptorArray::number_of_entries() const {
  return number_of_descriptors();
}

inline int16_t DescriptorArray::CompareAndSwapRawNumberOfMarkedDescriptors(
    int16_t expected, int16_t value) {
  return base::Relaxed_CompareAndSwap(
      reinterpret_cast<base::Atomic16*>(
          FIELD_ADDR(this, kRawNumberOfMarkedDescriptorsOffset)),
      expected, value);
}

void DescriptorArray::CopyEnumCacheFrom(DescriptorArray array) {
  set_enum_cache(array->enum_cache());
}

int DescriptorArray::Search(Name name, int valid_descriptors) {
  DCHECK(name->IsUniqueName());
  return internal::Search<VALID_ENTRIES>(this, name, valid_descriptors,
                                         nullptr);
}

int DescriptorArray::Search(Name name, Map map) {
  DCHECK(name->IsUniqueName());
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  if (number_of_own_descriptors == 0) return kNotFound;
  return Search(name, number_of_own_descriptors);
}

int DescriptorArray::SearchWithCache(Isolate* isolate, Name name, Map map) {
  DCHECK(name->IsUniqueName());
  int number_of_own_descriptors = map->NumberOfOwnDescriptors();
  if (number_of_own_descriptors == 0) return kNotFound;

  DescriptorLookupCache* cache = isolate->descriptor_lookup_cache();
  int number = cache->Lookup(map, name);

  if (number == DescriptorLookupCache::kAbsent) {
    number = Search(name, number_of_own_descriptors);
    cache->Update(map, name, number);
  }

  return number;
}

ObjectSlot DescriptorArray::GetFirstPointerSlot() {
  return RawField(DescriptorArray::kPointersStartOffset);
}

ObjectSlot DescriptorArray::GetDescriptorSlot(int descriptor) {
  // Allow descriptor == number_of_all_descriptors() for computing the slot
  // address that comes after the last descriptor (for iterating).
  DCHECK_LE(descriptor, number_of_all_descriptors());
  return RawField(OffsetOfDescriptorAt(descriptor));
}

ObjectSlot DescriptorArray::GetKeySlot(int descriptor) {
  DCHECK_LE(descriptor, number_of_all_descriptors());
  ObjectSlot slot = GetDescriptorSlot(descriptor) + kEntryKeyIndex;
  DCHECK((*slot)->IsObject());
  return slot;
}

Name DescriptorArray::GetKey(int descriptor_number) const {
  DCHECK(descriptor_number < number_of_descriptors());
  return Name::cast(
      get(ToKeyIndex(descriptor_number))->GetHeapObjectAssumeStrong());
}

int DescriptorArray::GetSortedKeyIndex(int descriptor_number) {
  return GetDetails(descriptor_number).pointer();
}

Name DescriptorArray::GetSortedKey(int descriptor_number) {
  return GetKey(GetSortedKeyIndex(descriptor_number));
}

void DescriptorArray::SetSortedKey(int descriptor_index, int pointer) {
  PropertyDetails details = GetDetails(descriptor_index);
  set(ToDetailsIndex(descriptor_index),
      MaybeObject::FromObject(details.set_pointer(pointer).AsSmi()));
}

MaybeObjectSlot DescriptorArray::GetValueSlot(int descriptor) {
  DCHECK_LT(descriptor, number_of_descriptors());
  return MaybeObjectSlot(GetDescriptorSlot(descriptor) + kEntryValueIndex);
}

Object DescriptorArray::GetStrongValue(int descriptor_number) {
  DCHECK(descriptor_number < number_of_descriptors());
  return get(ToValueIndex(descriptor_number))->cast<Object>();
}

void DescriptorArray::SetValue(int descriptor_index, Object value) {
  set(ToValueIndex(descriptor_index), MaybeObject::FromObject(value));
}

MaybeObject DescriptorArray::GetValue(int descriptor_number) {
  DCHECK_LT(descriptor_number, number_of_descriptors());
  return get(ToValueIndex(descriptor_number));
}

PropertyDetails DescriptorArray::GetDetails(int descriptor_number) {
  DCHECK(descriptor_number < number_of_descriptors());
  MaybeObject details = get(ToDetailsIndex(descriptor_number));
  return PropertyDetails(details->ToSmi());
}

int DescriptorArray::GetFieldIndex(int descriptor_number) {
  DCHECK_EQ(GetDetails(descriptor_number).location(), kField);
  return GetDetails(descriptor_number).field_index();
}

FieldType DescriptorArray::GetFieldType(int descriptor_number) {
  DCHECK_EQ(GetDetails(descriptor_number).location(), kField);
  MaybeObject wrapped_type = GetValue(descriptor_number);
  return Map::UnwrapFieldType(wrapped_type);
}

void DescriptorArray::Set(int descriptor_number, Name key, MaybeObject value,
                          PropertyDetails details) {
  // Range check.
  DCHECK(descriptor_number < number_of_descriptors());
  set(ToKeyIndex(descriptor_number), MaybeObject::FromObject(key));
  set(ToValueIndex(descriptor_number), value);
  set(ToDetailsIndex(descriptor_number),
      MaybeObject::FromObject(details.AsSmi()));
}

void DescriptorArray::Set(int descriptor_number, Descriptor* desc) {
  Name key = *desc->GetKey();
  MaybeObject value = *desc->GetValue();
  Set(descriptor_number, key, value, desc->GetDetails());
}

void DescriptorArray::Append(Descriptor* desc) {
  DisallowHeapAllocation no_gc;
  int descriptor_number = number_of_descriptors();
  DCHECK_LE(descriptor_number + 1, number_of_all_descriptors());
  set_number_of_descriptors(descriptor_number + 1);
  Set(descriptor_number, desc);

  uint32_t hash = desc->GetKey()->Hash();

  int insertion;

  for (insertion = descriptor_number; insertion > 0; --insertion) {
    Name key = GetSortedKey(insertion - 1);
    if (key->Hash() <= hash) break;
    SetSortedKey(insertion, GetSortedKeyIndex(insertion - 1));
  }

  SetSortedKey(insertion, descriptor_number);
}

void DescriptorArray::SwapSortedKeys(int first, int second) {
  int first_key = GetSortedKeyIndex(first);
  SetSortedKey(first, GetSortedKeyIndex(second));
  SetSortedKey(second, first_key);
}

int DescriptorArray::length() const {
  return number_of_all_descriptors() * kEntrySize;
}

MaybeObject DescriptorArray::get(int index) const {
  DCHECK(index >= 0 && index < this->length());
  return RELAXED_READ_WEAK_FIELD(*this, offset(index));
}

void DescriptorArray::set(int index, MaybeObject value) {
  DCHECK(index >= 0 && index < this->length());
  RELAXED_WRITE_WEAK_FIELD(*this, offset(index), value);
  WEAK_WRITE_BARRIER(*this, offset(index), value);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DESCRIPTOR_ARRAY_INL_H_
