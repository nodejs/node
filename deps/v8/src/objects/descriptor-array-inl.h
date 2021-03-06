// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DESCRIPTOR_ARRAY_INL_H_
#define V8_OBJECTS_DESCRIPTOR_ARRAY_INL_H_

#include "src/objects/descriptor-array.h"

#include "src/execution/isolate.h"
#include "src/handles/maybe-handles-inl.h"
#include "src/heap/heap-write-barrier.h"
#include "src/heap/heap.h"
#include "src/objects/field-type.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/lookup-cache-inl.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/property.h"
#include "src/objects/struct-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/descriptor-array-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(DescriptorArray)
TQ_OBJECT_CONSTRUCTORS_IMPL(EnumCache)

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
          FIELD_ADDR(*this, kRawNumberOfMarkedDescriptorsOffset)),
      expected, value);
}

void DescriptorArray::CopyEnumCacheFrom(DescriptorArray array) {
  set_enum_cache(array.enum_cache());
}

InternalIndex DescriptorArray::Search(Name name, int valid_descriptors,
                                      bool concurrent_search) {
  DCHECK(name.IsUniqueName());
  return InternalIndex(internal::Search<VALID_ENTRIES>(
      this, name, valid_descriptors, nullptr, concurrent_search));
}

InternalIndex DescriptorArray::Search(Name name, Map map,
                                      bool concurrent_search) {
  DCHECK(name.IsUniqueName());
  int number_of_own_descriptors = map.NumberOfOwnDescriptors();
  if (number_of_own_descriptors == 0) return InternalIndex::NotFound();
  return Search(name, number_of_own_descriptors, concurrent_search);
}

InternalIndex DescriptorArray::SearchWithCache(Isolate* isolate, Name name,
                                               Map map) {
  DCHECK(name.IsUniqueName());
  int number_of_own_descriptors = map.NumberOfOwnDescriptors();
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
  static_assert(kEndOfStrongFieldsOffset == kStartOfWeakFieldsOffset,
                "Weak and strong fields are continuous.");
  static_assert(kEndOfWeakFieldsOffset == kHeaderSize,
                "Weak fields extend up to the end of the header.");
  return RawField(DescriptorArray::kStartOfStrongFieldsOffset);
}

ObjectSlot DescriptorArray::GetDescriptorSlot(int descriptor) {
  // Allow descriptor == number_of_all_descriptors() for computing the slot
  // address that comes after the last descriptor (for iterating).
  DCHECK_LE(descriptor, number_of_all_descriptors());
  return RawField(OffsetOfDescriptorAt(descriptor));
}

Name DescriptorArray::GetKey(InternalIndex descriptor_number) const {
  IsolateRoot isolate = GetIsolateForPtrCompr(*this);
  return GetKey(isolate, descriptor_number);
}

Name DescriptorArray::GetKey(IsolateRoot isolate,
                             InternalIndex descriptor_number) const {
  DCHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  int entry_offset = OffsetOfDescriptorAt(descriptor_number.as_int());
  return Name::cast(EntryKeyField::Relaxed_Load(isolate, *this, entry_offset));
}

void DescriptorArray::SetKey(InternalIndex descriptor_number, Name key) {
  DCHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  int entry_offset = OffsetOfDescriptorAt(descriptor_number.as_int());
  EntryKeyField::Relaxed_Store(*this, entry_offset, key);
  WRITE_BARRIER(*this, entry_offset + kEntryKeyOffset, key);
}

int DescriptorArray::GetSortedKeyIndex(int descriptor_number) {
  return GetDetails(InternalIndex(descriptor_number)).pointer();
}

Name DescriptorArray::GetSortedKey(int descriptor_number) {
  IsolateRoot isolate = GetIsolateForPtrCompr(*this);
  return GetSortedKey(isolate, descriptor_number);
}

Name DescriptorArray::GetSortedKey(IsolateRoot isolate, int descriptor_number) {
  return GetKey(isolate, InternalIndex(GetSortedKeyIndex(descriptor_number)));
}

void DescriptorArray::SetSortedKey(int descriptor_number, int pointer) {
  PropertyDetails details = GetDetails(InternalIndex(descriptor_number));
  SetDetails(InternalIndex(descriptor_number), details.set_pointer(pointer));
}

Object DescriptorArray::GetStrongValue(InternalIndex descriptor_number) {
  IsolateRoot isolate = GetIsolateForPtrCompr(*this);
  return GetStrongValue(isolate, descriptor_number);
}

Object DescriptorArray::GetStrongValue(IsolateRoot isolate,
                                       InternalIndex descriptor_number) {
  return GetValue(isolate, descriptor_number).cast<Object>();
}

void DescriptorArray::SetValue(InternalIndex descriptor_number,
                               MaybeObject value) {
  DCHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  int entry_offset = OffsetOfDescriptorAt(descriptor_number.as_int());
  EntryValueField::Relaxed_Store(*this, entry_offset, value);
  WEAK_WRITE_BARRIER(*this, entry_offset + kEntryValueOffset, value);
}

MaybeObject DescriptorArray::GetValue(InternalIndex descriptor_number) {
  IsolateRoot isolate = GetIsolateForPtrCompr(*this);
  return GetValue(isolate, descriptor_number);
}

MaybeObject DescriptorArray::GetValue(IsolateRoot isolate,
                                      InternalIndex descriptor_number) {
  DCHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  int entry_offset = OffsetOfDescriptorAt(descriptor_number.as_int());
  return EntryValueField::Relaxed_Load(isolate, *this, entry_offset);
}

PropertyDetails DescriptorArray::GetDetails(InternalIndex descriptor_number) {
  DCHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  int entry_offset = OffsetOfDescriptorAt(descriptor_number.as_int());
  Smi details = EntryDetailsField::Relaxed_Load(*this, entry_offset);
  return PropertyDetails(details);
}

void DescriptorArray::SetDetails(InternalIndex descriptor_number,
                                 PropertyDetails details) {
  DCHECK_LT(descriptor_number.as_int(), number_of_descriptors());
  int entry_offset = OffsetOfDescriptorAt(descriptor_number.as_int());
  EntryDetailsField::Relaxed_Store(*this, entry_offset, details.AsSmi());
}

int DescriptorArray::GetFieldIndex(InternalIndex descriptor_number) {
  DCHECK_EQ(GetDetails(descriptor_number).location(), kField);
  return GetDetails(descriptor_number).field_index();
}

FieldType DescriptorArray::GetFieldType(InternalIndex descriptor_number) {
  IsolateRoot isolate = GetIsolateForPtrCompr(*this);
  return GetFieldType(isolate, descriptor_number);
}

FieldType DescriptorArray::GetFieldType(IsolateRoot isolate,
                                        InternalIndex descriptor_number) {
  DCHECK_EQ(GetDetails(descriptor_number).location(), kField);
  MaybeObject wrapped_type = GetValue(isolate, descriptor_number);
  return Map::UnwrapFieldType(wrapped_type);
}

void DescriptorArray::Set(InternalIndex descriptor_number, Name key,
                          MaybeObject value, PropertyDetails details) {
  SetKey(descriptor_number, key);
  SetDetails(descriptor_number, details);
  SetValue(descriptor_number, value);
}

void DescriptorArray::Set(InternalIndex descriptor_number, Descriptor* desc) {
  Name key = *desc->GetKey();
  MaybeObject value = *desc->GetValue();
  Set(descriptor_number, key, value, desc->GetDetails());
}

void DescriptorArray::Append(Descriptor* desc) {
  DisallowGarbageCollection no_gc;
  int descriptor_number = number_of_descriptors();
  DCHECK_LE(descriptor_number + 1, number_of_all_descriptors());
  set_number_of_descriptors(descriptor_number + 1);
  Set(InternalIndex(descriptor_number), desc);

  uint32_t hash = desc->GetKey()->hash();

  int insertion;

  for (insertion = descriptor_number; insertion > 0; --insertion) {
    Name key = GetSortedKey(insertion - 1);
    if (key.hash() <= hash) break;
    SetSortedKey(insertion, GetSortedKeyIndex(insertion - 1));
  }

  SetSortedKey(insertion, descriptor_number);
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
