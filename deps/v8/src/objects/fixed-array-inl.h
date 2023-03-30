// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_FIXED_ARRAY_INL_H_
#define V8_OBJECTS_FIXED_ARRAY_INL_H_

#include "src/handles/handles-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/numbers/conversions.h"
#include "src/objects/bigint.h"
#include "src/objects/compressed-slots.h"
#include "src/objects/fixed-array.h"
#include "src/objects/map.h"
#include "src/objects/maybe-object-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/slots.h"
#include "src/roots/roots-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/fixed-array-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(FixedArrayBase)
FixedArrayBase::FixedArrayBase(Address ptr,
                               HeapObject::AllowInlineSmiStorage allow_smi)
    : TorqueGeneratedFixedArrayBase(ptr, allow_smi) {}
TQ_OBJECT_CONSTRUCTORS_IMPL(FixedArray)
TQ_OBJECT_CONSTRUCTORS_IMPL(FixedDoubleArray)
TQ_OBJECT_CONSTRUCTORS_IMPL(ArrayList)
TQ_OBJECT_CONSTRUCTORS_IMPL(ByteArray)
ByteArray::ByteArray(Address ptr, HeapObject::AllowInlineSmiStorage allow_smi)
    : TorqueGeneratedByteArray(ptr, allow_smi) {}
TQ_OBJECT_CONSTRUCTORS_IMPL(TemplateList)
TQ_OBJECT_CONSTRUCTORS_IMPL(WeakFixedArray)
TQ_OBJECT_CONSTRUCTORS_IMPL(WeakArrayList)

NEVER_READ_ONLY_SPACE_IMPL(WeakArrayList)

RELEASE_ACQUIRE_SMI_ACCESSORS(FixedArrayBase, length, kLengthOffset)

RELEASE_ACQUIRE_SMI_ACCESSORS(WeakFixedArray, length, kLengthOffset)

Object FixedArrayBase::unchecked_length(AcquireLoadTag) const {
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
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return get(cage_base, index);
}

Object FixedArray::get(PtrComprCageBase cage_base, int index) const {
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  return TaggedField<Object>::Relaxed_Load(cage_base, *this,
                                           OffsetOfElementAt(index));
}

Handle<Object> FixedArray::get(FixedArray array, int index, Isolate* isolate) {
  return handle(array.get(isolate, index), isolate);
}

bool FixedArray::is_the_hole(Isolate* isolate, int index) {
  return get(isolate, index).IsTheHole(isolate);
}

void FixedArray::set(int index, Smi value) {
  DCHECK_NE(map(), EarlyGetReadOnlyRoots().unchecked_fixed_cow_array_map());
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  DCHECK(Object(value).IsSmi());
  int offset = OffsetOfElementAt(index);
  RELAXED_WRITE_FIELD(*this, offset, value);
}

void FixedArray::set(int index, Object value) {
  DCHECK_NE(EarlyGetReadOnlyRoots().unchecked_fixed_cow_array_map(), map());
  DCHECK(IsFixedArray());
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  int offset = OffsetOfElementAt(index);
  RELAXED_WRITE_FIELD(*this, offset, value);
  WRITE_BARRIER(*this, offset, value);
}

void FixedArray::set(int index, Object value, WriteBarrierMode mode) {
  DCHECK_NE(map(), GetReadOnlyRoots().fixed_cow_array_map());
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  int offset = OffsetOfElementAt(index);
  RELAXED_WRITE_FIELD(*this, offset, value);
  CONDITIONAL_WRITE_BARRIER(*this, offset, value, mode);
}

// static
void FixedArray::NoWriteBarrierSet(FixedArray array, int index, Object value) {
  DCHECK_NE(array.map(), array.GetReadOnlyRoots().fixed_cow_array_map());
  DCHECK_LT(static_cast<unsigned>(index),
            static_cast<unsigned>(array.length()));
  DCHECK(!ObjectInYoungGeneration(value));
  int offset = OffsetOfElementAt(index);
  RELAXED_WRITE_FIELD(array, offset, value);
}

Object FixedArray::get(int index, RelaxedLoadTag) const {
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return get(cage_base, index);
}

Object FixedArray::get(PtrComprCageBase cage_base, int index,
                       RelaxedLoadTag) const {
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  return RELAXED_READ_FIELD(*this, OffsetOfElementAt(index));
}

void FixedArray::set(int index, Object value, RelaxedStoreTag,
                     WriteBarrierMode mode) {
  DCHECK_NE(map(), GetReadOnlyRoots().fixed_cow_array_map());
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  RELAXED_WRITE_FIELD(*this, OffsetOfElementAt(index), value);
  CONDITIONAL_WRITE_BARRIER(*this, OffsetOfElementAt(index), value, mode);
}

void FixedArray::set(int index, Smi value, RelaxedStoreTag tag) {
  DCHECK(Object(value).IsSmi());
  set(index, value, tag, SKIP_WRITE_BARRIER);
}

Object FixedArray::get(int index, SeqCstAccessTag) const {
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return get(cage_base, index);
}

Object FixedArray::get(PtrComprCageBase cage_base, int index,
                       SeqCstAccessTag) const {
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  return SEQ_CST_READ_FIELD(*this, OffsetOfElementAt(index));
}

void FixedArray::set(int index, Object value, SeqCstAccessTag,
                     WriteBarrierMode mode) {
  DCHECK_NE(map(), GetReadOnlyRoots().fixed_cow_array_map());
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  SEQ_CST_WRITE_FIELD(*this, OffsetOfElementAt(index), value);
  CONDITIONAL_WRITE_BARRIER(*this, OffsetOfElementAt(index), value, mode);
}

void FixedArray::set(int index, Smi value, SeqCstAccessTag tag) {
  DCHECK(Object(value).IsSmi());
  set(index, value, tag, SKIP_WRITE_BARRIER);
}

Object FixedArray::get(int index, AcquireLoadTag) const {
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return get(cage_base, index);
}

Object FixedArray::get(PtrComprCageBase cage_base, int index,
                       AcquireLoadTag) const {
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  return ACQUIRE_READ_FIELD(*this, OffsetOfElementAt(index));
}

void FixedArray::set(int index, Object value, ReleaseStoreTag,
                     WriteBarrierMode mode) {
  DCHECK_NE(map(), GetReadOnlyRoots().fixed_cow_array_map());
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  RELEASE_WRITE_FIELD(*this, OffsetOfElementAt(index), value);
  CONDITIONAL_WRITE_BARRIER(*this, OffsetOfElementAt(index), value, mode);
}

void FixedArray::set(int index, Smi value, ReleaseStoreTag tag) {
  DCHECK(Object(value).IsSmi());
  set(index, value, tag, SKIP_WRITE_BARRIER);
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

Object FixedArray::swap(int index, Object value, SeqCstAccessTag,
                        WriteBarrierMode mode) {
  DCHECK_NE(map(), GetReadOnlyRoots().fixed_cow_array_map());
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  Object previous_value =
      SEQ_CST_SWAP_FIELD(*this, OffsetOfElementAt(index), value);
  CONDITIONAL_WRITE_BARRIER(*this, OffsetOfElementAt(index), value, mode);
  return previous_value;
}

Object FixedArray::swap(int index, Smi value, SeqCstAccessTag tag) {
  DCHECK(Object(value).IsSmi());
  return swap(index, value, tag, SKIP_WRITE_BARRIER);
}

void FixedArray::FillWithHoles(int from, int to) {
  for (int i = from; i < to; i++) {
    set_the_hole(i);
  }
}

ObjectSlot FixedArray::data_start() { return RawField(OffsetOfElementAt(0)); }

ObjectSlot FixedArray::RawFieldOfElementAt(int index) {
  return RawField(OffsetOfElementAt(index));
}

void FixedArray::MoveElements(Isolate* isolate, int dst_index, int src_index,
                              int len, WriteBarrierMode mode) {
  if (len == 0) return;
  DCHECK_LE(dst_index + len, length());
  DCHECK_LE(src_index + len, length());
  DisallowGarbageCollection no_gc;
  ObjectSlot dst_slot(RawFieldOfElementAt(dst_index));
  ObjectSlot src_slot(RawFieldOfElementAt(src_index));
  isolate->heap()->MoveRange(*this, dst_slot, src_slot, len, mode);
}

void FixedArray::CopyElements(Isolate* isolate, int dst_index, FixedArray src,
                              int src_index, int len, WriteBarrierMode mode) {
  if (len == 0) return;
  DCHECK_LE(dst_index + len, length());
  DCHECK_LE(src_index + len, src.length());
  DisallowGarbageCollection no_gc;

  ObjectSlot dst_slot(RawFieldOfElementAt(dst_index));
  ObjectSlot src_slot(src.RawFieldOfElementAt(src_index));
  isolate->heap()->CopyRange(*this, dst_slot, src_slot, len, mode);
}

// Due to left- and right-trimming, concurrent visitors need to read the length
// with acquire semantics.
// TODO(ulan): Acquire should not be needed anymore.
inline int FixedArray::AllocatedSize() { return SizeFor(length(kAcquireLoad)); }
inline int WeakFixedArray::AllocatedSize() {
  return SizeFor(length(kAcquireLoad));
}
inline int WeakArrayList::AllocatedSize() { return SizeFor(capacity()); }

// Perform a binary search in a fixed array.
template <SearchMode search_mode, typename T>
int BinarySearch(T* array, Name name, int valid_entries,
                 int* out_insertion_index) {
  DCHECK_IMPLIES(search_mode == VALID_ENTRIES, out_insertion_index == nullptr);
  int low = 0;
  // We have to search on all entries, even when search_mode == VALID_ENTRIES.
  // This is because the InternalIndex might be different from the SortedIndex
  // (i.e the first added item in {array} could be the last in the sorted
  // index). After doing the binary search and getting the correct internal
  // index we check to have the index lower than valid_entries, if needed.
  int high = array->number_of_entries() - 1;
  uint32_t hash = name.hash();
  int limit = high;

  DCHECK(low <= high);

  while (low != high) {
    int mid = low + (high - low) / 2;
    Name mid_name = array->GetSortedKey(mid);
    uint32_t mid_hash = mid_name.hash();

    if (mid_hash >= hash) {
      high = mid;
    } else {
      low = mid + 1;
    }
  }

  for (; low <= limit; ++low) {
    int sort_index = array->GetSortedKeyIndex(low);
    Name entry = array->GetKey(InternalIndex(sort_index));
    uint32_t current_hash = entry.hash();
    if (current_hash != hash) {
      // 'search_mode == ALL_ENTRIES' here and below is not needed since
      // 'out_insertion_index != nullptr' implies 'search_mode == ALL_ENTRIES'.
      // Having said that, when creating the template for <VALID_ENTRIES> these
      // ifs can be elided by the C++ compiler if we add 'search_mode ==
      // ALL_ENTRIES'.
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
    uint32_t hash = name.hash();
    int len = array->number_of_entries();
    for (int number = 0; number < len; number++) {
      int sorted_index = array->GetSortedKeyIndex(number);
      Name entry = array->GetKey(InternalIndex(sorted_index));
      uint32_t current_hash = entry.hash();
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
      if (array->GetKey(InternalIndex(number)) == name) return number;
    }
    return T::kNotFound;
  }
}

template <SearchMode search_mode, typename T>
int Search(T* array, Name name, int valid_entries, int* out_insertion_index,
           bool concurrent_search) {
  SLOW_DCHECK_IMPLIES(!concurrent_search, array->IsSortedNoDuplicates());

  if (valid_entries == 0) {
    if (search_mode == ALL_ENTRIES && out_insertion_index != nullptr) {
      *out_insertion_index = 0;
    }
    return T::kNotFound;
  }

  // Do linear search for small arrays, and for searches in the background
  // thread.
  const int kMaxElementsForLinearSearch = 8;
  if (valid_entries <= kMaxElementsForLinearSearch || concurrent_search) {
    return LinearSearch<search_mode>(array, name, valid_entries,
                                     out_insertion_index);
  }

  return BinarySearch<search_mode>(array, name, valid_entries,
                                   out_insertion_index);
}

double FixedDoubleArray::get_scalar(int index) {
  DCHECK(map() != GetReadOnlyRoots().fixed_cow_array_map() &&
         map() != GetReadOnlyRoots().fixed_array_map());
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  DCHECK(!is_the_hole(index));
  return ReadField<double>(kHeaderSize + index * kDoubleSize);
}

uint64_t FixedDoubleArray::get_representation(int index) {
  DCHECK(map() != GetReadOnlyRoots().fixed_cow_array_map() &&
         map() != GetReadOnlyRoots().fixed_array_map());
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  int offset = kHeaderSize + index * kDoubleSize;
  // Bug(v8:8875): Doubles may be unaligned.
  return base::ReadUnalignedValue<uint64_t>(field_address(offset));
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
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
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
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  int offset = kHeaderSize + index * kDoubleSize;
  base::WriteUnalignedValue<uint64_t>(field_address(offset), kHoleNanInt64);
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
  double* data_start = reinterpret_cast<double*>(field_address(kHeaderSize));
  MemMove(data_start + dst_index, data_start + src_index, len * kDoubleSize);
}

void FixedDoubleArray::FillWithHoles(int from, int to) {
  for (int i = from; i < to; i++) {
    set_the_hole(i);
  }
}

MaybeObject WeakFixedArray::Get(int index) const {
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return Get(cage_base, index);
}

MaybeObject WeakFixedArray::Get(PtrComprCageBase cage_base, int index) const {
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(length()));
  return objects(cage_base, index, kRelaxedLoad);
}

void WeakFixedArray::Set(int index, MaybeObject value, WriteBarrierMode mode) {
  set_objects(index, value, mode);
}

Handle<WeakFixedArray> WeakFixedArray::EnsureSpace(Isolate* isolate,
                                                   Handle<WeakFixedArray> array,
                                                   int length) {
  if (array->length() < length) {
    int grow_by = length - array->length();
    array = isolate->factory()->CopyWeakFixedArrayAndGrow(array, grow_by);
  }
  return array;
}

MaybeObjectSlot WeakFixedArray::data_start() {
  return RawMaybeWeakField(kObjectsOffset);
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
  DisallowGarbageCollection no_gc;

  MaybeObjectSlot dst_slot(data_start() + dst_index);
  MaybeObjectSlot src_slot(src.data_start() + src_index);
  isolate->heap()->CopyRange(*this, dst_slot, src_slot, len, mode);
}

MaybeObject WeakArrayList::Get(int index) const {
  PtrComprCageBase cage_base = GetPtrComprCageBase(*this);
  return Get(cage_base, index);
}

MaybeObject WeakArrayList::Get(PtrComprCageBase cage_base, int index) const {
  DCHECK_LT(static_cast<unsigned>(index), static_cast<unsigned>(capacity()));
  return objects(cage_base, index, kRelaxedLoad);
}

void WeakArrayList::Set(int index, MaybeObject value, WriteBarrierMode mode) {
  set_objects(index, value, mode);
}

void WeakArrayList::Set(int index, Smi value) {
  Set(index, MaybeObject::FromSmi(value), SKIP_WRITE_BARRIER);
}

MaybeObjectSlot WeakArrayList::data_start() {
  return RawMaybeWeakField(kObjectsOffset);
}

void WeakArrayList::CopyElements(Isolate* isolate, int dst_index,
                                 WeakArrayList src, int src_index, int len,
                                 WriteBarrierMode mode) {
  if (len == 0) return;
  DCHECK_LE(dst_index + len, capacity());
  DCHECK_LE(src_index + len, src.capacity());
  DisallowGarbageCollection no_gc;

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

Object ArrayList::Get(PtrComprCageBase cage_base, int index) const {
  return FixedArray::cast(*this).get(cage_base, kFirstIndex + index);
}

ObjectSlot ArrayList::Slot(int index) {
  return RawField(OffsetOfElementAt(kFirstIndex + index));
}

void ArrayList::Set(int index, Object obj, WriteBarrierMode mode) {
  FixedArray::cast(*this).set(kFirstIndex + index, obj, mode);
}

void ArrayList::Set(int index, Smi value) {
  DCHECK(Object(value).IsSmi());
  Set(index, value, SKIP_WRITE_BARRIER);
}
void ArrayList::Clear(int index, Object undefined) {
  DCHECK(undefined.IsUndefined());
  FixedArray::cast(*this).set(kFirstIndex + index, undefined,
                              SKIP_WRITE_BARRIER);
}

int ByteArray::Size() { return RoundUp(length() + kHeaderSize, kTaggedSize); }

byte ByteArray::get(int offset) const {
  DCHECK_GE(offset, 0);
  DCHECK_LT(offset, length());
  return ReadField<byte>(kHeaderSize + offset);
}

void ByteArray::set(int offset, byte value) {
  DCHECK_GE(offset, 0);
  DCHECK_LT(offset, length());
  WriteField<byte>(kHeaderSize + offset, value);
}

int ByteArray::get_int(int offset) const {
  DCHECK_GE(offset, 0);
  DCHECK_LE(offset + sizeof(int), length());
  return ReadField<int>(kHeaderSize + offset);
}

void ByteArray::set_int(int offset, int value) {
  DCHECK_GE(offset, 0);
  DCHECK_LE(offset + sizeof(int), length());
  WriteField<int>(kHeaderSize + offset, value);
}

Address ByteArray::get_sandboxed_pointer(int offset) const {
  DCHECK_GE(offset, 0);
  DCHECK_LE(offset + sizeof(Address), length());
  PtrComprCageBase sandbox_base = GetPtrComprCageBase(*this);
  return ReadSandboxedPointerField(kHeaderSize + offset, sandbox_base);
}

void ByteArray::set_sandboxed_pointer(int offset, Address value) {
  DCHECK_GE(offset, 0);
  DCHECK_LE(offset + sizeof(Address), length());
  PtrComprCageBase sandbox_base = GetPtrComprCageBase(*this);
  WriteSandboxedPointerField(kHeaderSize + offset, sandbox_base, value);
}

void ByteArray::copy_in(int offset, const byte* buffer, int slice_length) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(slice_length, 0);
  DCHECK_LE(slice_length, kMaxInt - offset);
  DCHECK_LE(offset + slice_length, length());
  Address dst_addr = field_address(kHeaderSize + offset);
  memcpy(reinterpret_cast<void*>(dst_addr), buffer, slice_length);
}

void ByteArray::copy_out(int offset, byte* buffer, int slice_length) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(slice_length, 0);
  DCHECK_LE(slice_length, kMaxInt - offset);
  DCHECK_LE(offset + slice_length, length());
  Address src_addr = field_address(kHeaderSize + offset);
  memcpy(buffer, reinterpret_cast<void*>(src_addr), slice_length);
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

byte* ByteArray::GetDataStartAddress() {
  return reinterpret_cast<byte*>(address() + kHeaderSize);
}

byte* ByteArray::GetDataEndAddress() {
  return GetDataStartAddress() + length();
}

template <typename T>
FixedIntegerArray<T>::FixedIntegerArray(Address ptr) : ByteArray(ptr) {
  DCHECK_EQ(ByteArray::length() % sizeof(T), 0);
}

template <typename T>
FixedIntegerArray<T> FixedIntegerArray<T>::cast(Object object) {
  return FixedIntegerArray<T>(object.ptr());
}

// static
template <typename T>
Handle<FixedIntegerArray<T>> FixedIntegerArray<T>::New(
    Isolate* isolate, int length, AllocationType allocation) {
  int byte_length;
  CHECK(!base::bits::SignedMulOverflow32(length, sizeof(T), &byte_length));
  return Handle<FixedIntegerArray<T>>::cast(
      isolate->factory()->NewByteArray(byte_length, allocation));
}

template <typename T>
T FixedIntegerArray<T>::get(int index) const {
  static_assert(std::is_integral<T>::value);
  DCHECK_GE(index, 0);
  DCHECK_LT(index, length());
  return ReadField<T>(kHeaderSize + index * sizeof(T));
}

template <typename T>
void FixedIntegerArray<T>::set(int index, T value) {
  static_assert(std::is_integral<T>::value);
  DCHECK_GE(index, 0);
  DCHECK_LT(index, length());
  WriteField<T>(kHeaderSize + index * sizeof(T), value);
}

template <typename T>
int FixedIntegerArray<T>::length() const {
  DCHECK_EQ(ByteArray::length() % sizeof(T), 0);
  return ByteArray::length() / sizeof(T);
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
  int byte_length;
  CHECK(!base::bits::SignedMulOverflow32(length, sizeof(T), &byte_length));
  return Handle<PodArray<T>>::cast(
      isolate->factory()->NewByteArray(byte_length, allocation));
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

Object TemplateList::get(PtrComprCageBase cage_base, int index) const {
  return FixedArray::cast(*this).get(cage_base, kFirstElementIndex + index);
}

void TemplateList::set(int index, Object value) {
  FixedArray::cast(*this).set(kFirstElementIndex + index, value);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_FIXED_ARRAY_INL_H_
