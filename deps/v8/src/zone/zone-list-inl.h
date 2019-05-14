// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_LIST_INL_H_
#define V8_ZONE_ZONE_LIST_INL_H_

#include "src/zone/zone.h"

#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/memcopy.h"

namespace v8 {
namespace internal {

template <typename T>
void ZoneList<T>::Add(const T& element, Zone* zone) {
  if (length_ < capacity_) {
    data_[length_++] = element;
  } else {
    ZoneList<T>::ResizeAdd(element, ZoneAllocationPolicy(zone));
  }
}

template <typename T>
void ZoneList<T>::AddAll(const ZoneList<T>& other, Zone* zone) {
  AddAll(other.ToVector(), zone);
}

template <typename T>
void ZoneList<T>::AddAll(const Vector<T>& other, Zone* zone) {
  int result_length = length_ + other.length();
  if (capacity_ < result_length)
    Resize(result_length, ZoneAllocationPolicy(zone));
  if (std::is_fundamental<T>()) {
    memcpy(data_ + length_, other.start(), sizeof(*data_) * other.length());
  } else {
    for (int i = 0; i < other.length(); i++) data_[length_ + i] = other.at(i);
  }
  length_ = result_length;
}

// Use two layers of inlining so that the non-inlined function can
// use the same implementation as the inlined version.
template <typename T>
void ZoneList<T>::ResizeAdd(const T& element, ZoneAllocationPolicy alloc) {
  ResizeAddInternal(element, alloc);
}

template <typename T>
void ZoneList<T>::ResizeAddInternal(const T& element,
                                    ZoneAllocationPolicy alloc) {
  DCHECK(length_ >= capacity_);
  // Grow the list capacity by 100%, but make sure to let it grow
  // even when the capacity is zero (possible initial case).
  int new_capacity = 1 + 2 * capacity_;
  // Since the element reference could be an element of the list, copy
  // it out of the old backing storage before resizing.
  T temp = element;
  Resize(new_capacity, alloc);
  data_[length_++] = temp;
}

template <typename T>
void ZoneList<T>::Resize(int new_capacity, ZoneAllocationPolicy alloc) {
  DCHECK_LE(length_, new_capacity);
  T* new_data = NewData(new_capacity, alloc);
  if (length_ > 0) {
    MemCopy(new_data, data_, length_ * sizeof(T));
  }
  ZoneList<T>::DeleteData(data_);
  data_ = new_data;
  capacity_ = new_capacity;
}

template <typename T>
Vector<T> ZoneList<T>::AddBlock(T value, int count, Zone* zone) {
  int start = length_;
  for (int i = 0; i < count; i++) Add(value, zone);
  return Vector<T>(&data_[start], count);
}

template <typename T>
void ZoneList<T>::Set(int index, const T& elm) {
  DCHECK(index >= 0 && index <= length_);
  data_[index] = elm;
}

template <typename T>
void ZoneList<T>::InsertAt(int index, const T& elm, Zone* zone) {
  DCHECK(index >= 0 && index <= length_);
  Add(elm, zone);
  for (int i = length_ - 1; i > index; --i) {
    data_[i] = data_[i - 1];
  }
  data_[index] = elm;
}

template <typename T>
T ZoneList<T>::Remove(int i) {
  T element = at(i);
  length_--;
  while (i < length_) {
    data_[i] = data_[i + 1];
    i++;
  }
  return element;
}

template <typename T>
void ZoneList<T>::Clear() {
  DeleteData(data_);
  // We don't call Initialize(0) since that requires passing a Zone,
  // which we don't really need.
  data_ = nullptr;
  capacity_ = 0;
  length_ = 0;
}

template <typename T>
void ZoneList<T>::Rewind(int pos) {
  DCHECK(0 <= pos && pos <= length_);
  length_ = pos;
}

template <typename T>
template <class Visitor>
void ZoneList<T>::Iterate(Visitor* visitor) {
  for (int i = 0; i < length_; i++) visitor->Apply(&data_[i]);
}

template <typename T>
template <typename CompareFunction>
void ZoneList<T>::Sort(CompareFunction cmp) {
  ToVector().Sort(cmp, 0, length_);
#ifdef DEBUG
  for (int i = 1; i < length_; i++) {
    DCHECK_LE(cmp(&data_[i - 1], &data_[i]), 0);
  }
#endif
}

template <typename T>
template <typename CompareFunction>
void ZoneList<T>::StableSort(CompareFunction cmp, size_t s, size_t l) {
  ToVector().StableSort(cmp, s, l);
#ifdef DEBUG
  for (size_t i = s + 1; i < l; i++) {
    DCHECK_LE(cmp(&data_[i - 1], &data_[i]), 0);
  }
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_LIST_INL_H_
