// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIXED_ARRAY_INL_H_
#define V8_OBJECTS_FIXED_ARRAY_INL_H_

#include "src/objects/fixed-array.h"

#include "src/handles/handles-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/numbers/conversions.h"
#include "src/objects/bigint.h"
#include "src/objects/compressed-slots.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/map.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/slots.h"
#include "src/roots/roots-inl.h"
#include "src/sanitizer/tsan.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(FixedArrayBase, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(FixedArray, FixedArrayBase)
OBJECT_CONSTRUCTORS_IMPL(FixedDoubleArray, FixedArrayBase)
OBJECT_CONSTRUCTORS_IMPL(ArrayList, FixedArray)
OBJECT_CONSTRUCTORS_IMPL(ByteArray, FixedArrayBase)
OBJECT_CONSTRUCTORS_IMPL(TemplateList, FixedArray)
OBJECT_CONSTRUCTORS_IMPL(WeakFixedArray, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(WeakArrayList, HeapObject)

FixedArrayBase::FixedArrayBase(Address ptr, AllowInlineSmiStorage allow_smi)
    : HeapObject(ptr, allow_smi) {
  SLOW_DCHECK(
      (allow_smi == AllowInlineSmiStorage::kAllowBeingASmi && IsSmi()) ||
      IsFixedArrayBase());
}

ByteArray::ByteArray(Address ptr, AllowInlineSmiStorage allow_smi)
    : FixedArrayBase(ptr, allow_smi) {
  SLOW_DCHECK(
      (allow_smi == AllowInlineSmiStorage::kAllowBeingASmi && IsSmi()) ||
      IsByteArray());
}

NEVER_READ_ONLY_SPACE_IMPL(WeakArrayList)

CAST_ACCESSOR(ArrayList)
CAST_ACCESSOR(ByteArray)
CAST_ACCESSOR(FixedArray)
CAST_ACCESSOR(FixedArrayBase)
CAST_ACCESSOR(FixedDoubleArray)
CAST_ACCESSOR(TemplateList)
CAST_ACCESSOR(WeakFixedArray)
CAST_ACCESSOR(WeakArrayList)

SMI_ACCESSORS(FixedArrayBase, length, kLengthOffset)
SYNCHRONIZED_SMI_ACCESSORS(FixedArrayBase, length, kLengthOffset)

SMI_ACCESSORS(WeakFixedArray, length, kLengthOffset)
SYNCHRONIZED_SMI_ACCESSORS(WeakFixedArray, length, kLengthOffset)

SMI_ACCESSORS(WeakArrayList, capacity, kCapacityOffset)
SYNCHRONIZED_SMI_ACCESSORS(WeakArrayList, capacity, kCapacityOffset)
SMI_ACCESSORS(WeakArrayList, length, kLengthOffset)

Object FixedArrayBase::unchecked_synchronized_length() const {
  return ACQUIRE_READ_FIELD(*this, kLengthOffset);
}

ObjectSlot FixedArray::GetFirstElementAddress() {
  return RawField(OffsetOfElementAt(0));
}

bool FixedArray::ContainsOnlySmisOrHoles() {
  Object the_hole = GetReadOnlyRoots().the_hole_value();
  ObjectSlot current = GetFirstElementAddress();
  for (int i = 0; i < length(); ++i, ++current) {
    Object candidate = *current;
    if (!candidate.IsSmi() && candidate != the_hole) return false;
  }
  return true;
}

Object FixedArray::get(int index) const {
  DCHECK(index >= 0 && index < this->length());
  return RELAXED_READ_FIELD(*this, kHeaderSize + index * kTaggedSize);
}

Handle<Object> FixedArray::get(FixedArray array, int index, Isolate* isolate) {
  return handle(array.get(index), isolate);
}

bool FixedArray::is_the_hole(Isolate* isolate, int index) {
  return get(index).IsTheHole(isolate);
}

void FixedArray::set(int index, Smi value) {
  DCHECK_NE(map(), GetReadOnlyRoots().fixed_cow_array_map());
  DCHECK_LT(index, this->length());
  DCHECK(Object(value).IsSmi());
  int offset = kHeaderSize + index * kTaggedSize;
  RELAXED_WRITE_FIELD(*this, offset, value);
}

void FixedArray::set(int index, Object value) {
  DCHECK_NE(GetReadOnlyRoots().fixed_cow_array_map(), map());
  DCHECK(IsFixedArray());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kHeaderSize + index * kTaggedSize;
  RELAXED_WRITE_FIELD(*this, offset, value);
  WRITE_BARRIER(*this, offset, value);
}

void FixedArray::set(int index, Object value, WriteBarrierMode mode) {
  DCHECK_NE(map(), GetReadOnlyRoots().fixed_cow_array_map());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->length());
  int offset = kHeaderSize + index * kTaggedSize;
  RELAXED_WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
}

void FixedArray::NoWriteBarrierSet(FixedArray array, int index, Object value) {
  DCHECK_NE(array.map(), array.GetReadOnlyRoots().fixed_cow_array_map());
  DCHECK_GE(index, 0);
  DCHECK_LT(index, array.length());
  DCHECK(!ObjectInYoungGeneration(value));
  RELAXED_WRITE_FIELD(array, kHeaderSize + index * kTaggedSize, value);
}

void FixedArray::set_undefined(int index) {
  set_undefined(GetReadOnlyRoots(), index);
}

void FixedArray::set_undefined(Isolate* isolate, int index) {
  set_undefined(ReadOnlyRoots(isolate), index);
}

void FixedArray::set_undefined(ReadOnlyRoots ro_roots, int index) {
  FixedArray::NoWriteBarrierSet(*this, index, ro_roots.undefined_value());
}

void FixedArray::set_null(int index) { set_null(GetReadOnlyRoots(), index); }

void FixedArray::set_null(Isolate* isolate, int index) {
  set_null(ReadOnlyRoots(isolate), index);
}

void FixedArray::set_null(ReadOnlyRoots ro_roots, int index) {
  FixedArray::NoWriteBarrierSet(*this, index, ro_roots.null_value());
}

void FixedArray::set_the_hole(int index) {
  set_the_hole(GetReadOnlyRoots(), index);
}

void FixedArray::set_the_hole(Isolate* isolate, int index) {
  set_the_hole(ReadOnlyRoots(isolate), index);
}

void FixedArray::set_the_hole(ReadOnlyRoots ro_roots, int index) {
  FixedArray::NoWriteBarrierSet(*this, index, ro_roots.the_hole_value());
}

void FixedArray::FillWithHoles(int from, int to) {
  for (int i = from; i < to; i++) {
    set_the_hole(i);
  }
}

ObjectSlot FixedArray::data_start() {
  return RawField(OffsetOfElementAt(0));
}

ObjectSlot FixedArray::RawFieldOfElementAt(int index) {
  return RawField(OffsetOfElementAt(index));
}

void FixedArray::MoveElements(Isolate* isolate, int dst_index, int src_index,
                              int len, WriteBarrierMode mode) {
  if (len == 0) return;
  DCHECK_LE(dst_index + len, length());
  DCHECK_LE(src_index + len, length());
  DisallowHeapAllocation no_gc;
  ObjectSlot dst_slot(RawFieldOfElementAt(dst_index));
  ObjectSlot src_slot(RawFieldOfElementAt(src_index));
  isolate->heap()->MoveRange(*this, dst_slot, src_slot, len, mode);
}

void FixedArray::CopyElements(Isolate* isolate, int dst_index, FixedArray src,
                              int src_index, int len, WriteBarrierMode mode) {
  if (len == 0) return;
  DCHECK_LE(dst_index + len, length());
  DCHECK_LE(src_index + len, src.length());
  DisallowHeapAllocation no_gc;

  ObjectSlot dst_slot(RawFieldOfElementAt(dst_index));
  ObjectSlot src_slot(src.RawFieldOfElementAt(src_index));
  isolate->heap()->CopyRange(*this, dst_slot, src_slot, len, mode);
}

// Perform a binary search in a fixed array.
template <SearchMode search_mode, typename T>
int BinarySearch(T* array, Name name, int valid_entries,
                 int* out_insertion_index) {
  DCHECK(search_mode == ALL_ENTRIES || out_insertion_index == nullptr);
  int low = 0;
  int high = array->number_of_entries() - 1;
  uint32_t hash = name.hash_field();
  int limit = high;

  DCHECK(low <= high);

  while (low != high) {
    int mid = low + (high - low) / 2;
    Name mid_name = array->GetSortedKey(mid);
    uint32_t mid_hash = mid_name.hash_field();

    if (mid_hash >= hash) {
      high = mid;
    } else {
      low = mid + 1;
    }
  }

  for (; low <= limit; ++low) {
    int sort_index = array->GetSortedKeyIndex(low);
    Name entry = array->GetKey(sort_index);
    uint32_t current_hash = entry.hash_field();
    if (current_hash != hash) {
      if (search_mode == ALL_ENTRIES && out_insertion_index != nullptr) {
        *out_insertion_index = sort_index + (current_hash > hash ? 0 : 1);
      }
      return T::kNotFound;
    }
    if (entry == name) {
      if (search_mode == ALL_ENTRIES || sort_index < valid_entries) {
        return sort_index;
      }
      return T::kNotFound;
    }
  }

  if (search_mode == ALL_ENTRIES && out_insertion_index != nullptr) {
    *out_insertion_index = limit + 1;
  }
  return T::kNotFound;
}

// Perform a linear search in this fixed array. len is the number of entry
// indices that are valid.
template <SearchMode search_mode, typename T>
int LinearSearch(T* array, Name name, int valid_entries,
                 int* out_insertion_index) {
  if (search_mode == ALL_ENTRIES && out_insertion_index != nullptr) {
    uint32_t hash = name.hash_field();
    int len = array->number_of_entries();
    for (int number = 0; number < len; number++) {
      int sorted_index = array->GetSortedKeyIndex(number);
      Name entry = array->GetKey(sorted_index);
      uint32_t current_hash = entry.hash_field();
      if (current_hash > hash) {
        *out_insertion_index = sorted_index;
        return T::kNotFound;
      }
      if (entry == name) return sorted_index;
    }
    *out_insertion_index = len;
    return T::kNotFound;
  } else {
    DCHECK_LE(valid_entries, array->number_of_entries());
    DCHECK_NULL(out_insertion_index);  // Not supported here.
    for (int number = 0; number < valid_entries; number++) {
      if (array->GetKey(number) == name) return number;
    }
    return T::kNotFound;
  }
}

template <SearchMode search_mode, typename T>
int Search(T* array, Name name, int valid_entries, int* out_insertion_index) {
  SLOW_DCHECK(array->IsSortedNoDuplicates());

  if (valid_entries == 0) {
    if (search_mode == ALL_ENTRIES && out_insertion_index != nullptr) {
      *out_insertion_index = 0;
    }
    return T::kNotFound;
  }

  // Fast case: do linear search for small arrays.
  const int kMaxElementsForLinearSearch = 8;
  if (valid_entries <= kMaxElementsForLinearSearch) {
    return LinearSearch<search_mode>(array, name, valid_entries,
                                     out_insertion_index);
  }

  // Slow case: perform binary search.
  return BinarySearch<search_mode>(array, name, valid_entries,
                                   out_insertion_index);
}

double FixedDoubleArray::get_scalar(int index) {
  DCHECK(map() != GetReadOnlyRoots().fixed_cow_array_map() &&
         map() != GetReadOnlyRoots().fixed_array_map());
  DCHECK(index >= 0 && index < this->length());
  DCHECK(!is_the_hole(index));
  return ReadField<double>(kHeaderSize + index * kDoubleSize);
}

uint64_t FixedDoubleArray::get_representation(int index) {
  DCHECK(map() != GetReadOnlyRoots().fixed_cow_array_map() &&
         map() != GetReadOnlyRoots().fixed_array_map());
  DCHECK(index >= 0 && index < this->length());
  int offset = kHeaderSize + index * kDoubleSize;
  // Bug(v8:8875): Doubles may be unaligned.
  return ReadUnalignedValue<uint64_t>(field_address(offset));
}

Handle<Object> FixedDoubleArray::get(FixedDoubleArray array, int index,
                                     Isolate* isolate) {
  if (array.is_the_hole(index)) {
    return ReadOnlyRoots(isolate).the_hole_value_handle();
  } else {
    return isolate->factory()->NewNumber(array.get_scalar(index));
  }
}

void FixedDoubleArray::set(int index, double value) {
  DCHECK(map() != GetReadOnlyRoots().fixed_cow_array_map() &&
         map() != GetReadOnlyRoots().fixed_array_map());
  int offset = kHeaderSize + index * kDoubleSize;
  if (std::isnan(value)) {
    WriteField<double>(offset, std::numeric_limits<double>::quiet_NaN());
  } else {
    WriteField<double>(offset, value);
  }
  DCHECK(!is_the_hole(index));
}

void FixedDoubleArray::set_the_hole(Isolate* isolate, int index) {
  set_the_hole(index);
}

void FixedDoubleArray::set_the_hole(int index) {
  DCHECK(map() != GetReadOnlyRoots().fixed_cow_array_map() &&
         map() != GetReadOnlyRoots().fixed_array_map());
  int offset = kHeaderSize + index * kDoubleSize;
  WriteUnalignedValue<uint64_t>(field_address(offset), kHoleNanInt64);
}

bool FixedDoubleArray::is_the_hole(Isolate* isolate, int index) {
  return is_the_hole(index);
}

bool FixedDoubleArray::is_the_hole(int index) {
  return get_representation(index) == kHoleNanInt64;
}

void FixedDoubleArray::MoveElements(Isolate* isolate, int dst_index,
                                    int src_index, int len,
                                    WriteBarrierMode mode) {
  DCHECK_EQ(SKIP_WRITE_BARRIER, mode);
  double* data_start =
      reinterpret_cast<double*>(FIELD_ADDR(*this, kHeaderSize));
  MemMove(data_start + dst_index, data_start + src_index, len * kDoubleSize);
}

void FixedDoubleArray::FillWithHoles(int from, int to) {
  for (int i = from; i < to; i++) {
    set_the_hole(i);
  }
}

MaybeObject WeakFixedArray::Get(int index) const {
  DCHECK(index >= 0 && index < this->length());
  return RELAXED_READ_WEAK_FIELD(*this, OffsetOfElementAt(index));
}

void WeakFixedArray::Set(int index, MaybeObject value) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, length());
  int offset = OffsetOfElementAt(index);
  RELAXED_WRITE_WEAK_FIELD(*this, offset, value);
  WEAK_WRITE_BARRIER(*this, offset, value);
}

void WeakFixedArray::Set(int index, MaybeObject value, WriteBarrierMode mode) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, length());
  int offset = OffsetOfElementAt(index);
  RELAXED_WRITE_WEAK_FIELD(*this, offset, value);
  CONDITIONAL_WEAK_WRITE_BARRIER(*this, offset, value, mode);
}

MaybeObjectSlot WeakFixedArray::data_start() {
  return RawMaybeWeakField(kHeaderSize);
}

MaybeObjectSlot WeakFixedArray::RawFieldOfElementAt(int index) {
  return RawMaybeWeakField(OffsetOfElementAt(index));
}

void WeakFixedArray::CopyElements(Isolate* isolate, int dst_index,
                                  WeakFixedArray src, int src_index, int len,
                                  WriteBarrierMode mode) {
  if (len == 0) return;
  DCHECK_LE(dst_index + len, length());
  DCHECK_LE(src_index + len, src.length());
  DisallowHeapAllocation no_gc;

  MaybeObjectSlot dst_slot(data_start() + dst_index);
  MaybeObjectSlot src_slot(src.data_start() + src_index);
  isolate->heap()->CopyRange(*this, dst_slot, src_slot, len, mode);
}

MaybeObject WeakArrayList::Get(int index) const {
  DCHECK(index >= 0 && index < this->capacity());
  return RELAXED_READ_WEAK_FIELD(*this, OffsetOfElementAt(index));
}

void WeakArrayList::Set(int index, MaybeObject value, WriteBarrierMode mode) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, this->capacity());
  int offset = OffsetOfElementAt(index);
  RELAXED_WRITE_WEAK_FIELD(*this, offset, value);
  CONDITIONAL_WEAK_WRITE_BARRIER(*this, offset, value, mode);
}

MaybeObjectSlot WeakArrayList::data_start() {
  return RawMaybeWeakField(kHeaderSize);
}

void WeakArrayList::CopyElements(Isolate* isolate, int dst_index,
                                 WeakArrayList src, int src_index, int len,
                                 WriteBarrierMode mode) {
  if (len == 0) return;
  DCHECK_LE(dst_index + len, capacity());
  DCHECK_LE(src_index + len, src.capacity());
  DisallowHeapAllocation no_gc;

  MaybeObjectSlot dst_slot(data_start() + dst_index);
  MaybeObjectSlot src_slot(src.data_start() + src_index);
  isolate->heap()->CopyRange(*this, dst_slot, src_slot, len, mode);
}

HeapObject WeakArrayList::Iterator::Next() {
  if (!array_.is_null()) {
    while (index_ < array_.length()) {
      MaybeObject item = array_.Get(index_++);
      DCHECK(item->IsWeakOrCleared());
      if (!item->IsCleared()) return item->GetHeapObjectAssumeWeak();
    }
    array_ = WeakArrayList();
  }
  return HeapObject();
}

int ArrayList::Length() const {
  if (FixedArray::cast(*this).length() == 0) return 0;
  return Smi::ToInt(FixedArray::cast(*this).get(kLengthIndex));
}

void ArrayList::SetLength(int length) {
  return FixedArray::cast(*this).set(kLengthIndex, Smi::FromInt(length));
}

Object ArrayList::Get(int index) const {
  return FixedArray::cast(*this).get(kFirstIndex + index);
}

ObjectSlot ArrayList::Slot(int index) {
  return RawField(OffsetOfElementAt(kFirstIndex + index));
}

void ArrayList::Set(int index, Object obj, WriteBarrierMode mode) {
  FixedArray::cast(*this).set(kFirstIndex + index, obj, mode);
}

void ArrayList::Clear(int index, Object undefined) {
  DCHECK(undefined.IsUndefined());
  FixedArray::cast(*this).set(kFirstIndex + index, undefined,
                              SKIP_WRITE_BARRIER);
}

int ByteArray::Size() { return RoundUp(length() + kHeaderSize, kTaggedSize); }

byte ByteArray::get(int index) const {
  DCHECK(index >= 0 && index < this->length());
  return ReadField<byte>(kHeaderSize + index * kCharSize);
}

void ByteArray::set(int index, byte value) {
  DCHECK(index >= 0 && index < this->length());
  WriteField<byte>(kHeaderSize + index * kCharSize, value);
}

void ByteArray::copy_in(int index, const byte* buffer, int length) {
  DCHECK(index >= 0 && length >= 0 && length <= kMaxInt - index &&
         index + length <= this->length());
  Address dst_addr = FIELD_ADDR(*this, kHeaderSize + index * kCharSize);
  memcpy(reinterpret_cast<void*>(dst_addr), buffer, length);
}

void ByteArray::copy_out(int index, byte* buffer, int length) {
  DCHECK(index >= 0 && length >= 0 && length <= kMaxInt - index &&
         index + length <= this->length());
  Address src_addr = FIELD_ADDR(*this, kHeaderSize + index * kCharSize);
  memcpy(buffer, reinterpret_cast<void*>(src_addr), length);
}

int ByteArray::get_int(int index) const {
  DCHECK(index >= 0 && index < this->length() / kIntSize);
  return ReadField<int>(kHeaderSize + index * kIntSize);
}

void ByteArray::set_int(int index, int value) {
  DCHECK(index >= 0 && index < this->length() / kIntSize);
  WriteField<int>(kHeaderSize + index * kIntSize, value);
}

uint32_t ByteArray::get_uint32(int index) const {
  DCHECK(index >= 0 && index < this->length() / kUInt32Size);
  return ReadField<uint32_t>(kHeaderSize + index * kUInt32Size);
}

void ByteArray::set_uint32(int index, uint32_t value) {
  DCHECK(index >= 0 && index < this->length() / kUInt32Size);
  WriteField<uint32_t>(kHeaderSize + index * kUInt32Size, value);
}

void ByteArray::clear_padding() {
  int data_size = length() + kHeaderSize;
  memset(reinterpret_cast<void*>(address() + data_size), 0, Size() - data_size);
}

ByteArray ByteArray::FromDataStartAddress(Address address) {
  DCHECK_TAG_ALIGNED(address);
  return ByteArray::cast(Object(address - kHeaderSize + kHeapObjectTag));
}

int ByteArray::DataSize() const { return RoundUp(length(), kTaggedSize); }

int ByteArray::ByteArraySize() { return SizeFor(this->length()); }

byte* ByteArray::GetDataStartAddress() {
  return reinterpret_cast<byte*>(address() + kHeaderSize);
}

byte* ByteArray::GetDataEndAddress() {
  return GetDataStartAddress() + length();
}

template <class T>
PodArray<T>::PodArray(Address ptr) : ByteArray(ptr) {}

template <class T>
PodArray<T> PodArray<T>::cast(Object object) {
  return PodArray<T>(object.ptr());
}

// static
template <class T>
Handle<PodArray<T>> PodArray<T>::New(Isolate* isolate, int length,
                                     AllocationType allocation) {
  return Handle<PodArray<T>>::cast(
      isolate->factory()->NewByteArray(length * sizeof(T), allocation));
}

template <class T>
int PodArray<T>::length() const {
  return ByteArray::length() / sizeof(T);
}

int TemplateList::length() const {
  return Smi::ToInt(FixedArray::cast(*this).get(kLengthIndex));
}

Object TemplateList::get(int index) const {
  return FixedArray::cast(*this).get(kFirstElementIndex + index);
}

void TemplateList::set(int index, Object value) {
  FixedArray::cast(*this).set(kFirstElementIndex + index, value);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FIXED_ARRAY_INL_H_
