// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_LIST_INL_H_
#define V8_ZONE_ZONE_LIST_INL_H_

#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/utils/memcopy.h"
#include "src/zone/zone-list.h"

namespace v8 {
namespace internal {

template <typename T>
void ZoneList<T>::Add(const T& element, Zone* zone) {
  if (length_ < capacity_) {
    data_[length_++] = element;
  } else {
    ZoneList<T>::ResizeAdd(element, zone);
  }
}

template <typename T>
void ZoneList<T>::AddAll(const ZoneList<T>& other, Zone* zone) {
  AddAll(other.ToVector(), zone);
}

template <typename T>
void ZoneList<T>::AddAll(base::Vector<const T> other, Zone* zone) {
  int length = other.length();
  if (length == 0) return;

  int result_length = length_ + length;
  if (capacity_ < result_length) Resize(result_length, zone);
  if (std::is_trivially_copyable<T>::value) {
    memcpy(&data_[length_], other.begin(), sizeof(T) * length);
  } else {
    std::copy(other.begin(), other.end(), &data_[length_]);
  }
  length_ = result_length;
}

// Use two layers of inlining so that the non-inlined function can
// use the same implementation as the inlined version.
template <typename T>
void ZoneList<T>::ResizeAdd(const T& element, Zone* zone) {
  ResizeAddInternal(element, zone);
}

template <typename T>
void ZoneList<T>::ResizeAddInternal(const T& element, Zone* zone) {
  DCHECK(length_ >= capacity_);
  // Grow the list capacity by 100%, but make sure to let it grow
  // even when the capacity is zero (possible initial case).
  int new_capacity = 1 + 2 * capacity_;
  // Since the element reference could be an element of the list, copy
  // it out of the old backing storage before resizing.
  T temp = element;
  Resize(new_capacity, zone);
  data_[length_++] = temp;
}

template <typename T>
void ZoneList<T>::Resize(int new_capacity, Zone* zone) {
  DCHECK_LE(length_, new_capacity);
  T* new_data = zone->AllocateArray<T>(new_capacity);
  if (length_ > 0) {
    if (std::is_trivially_copyable<T>::value) {
      MemCopy(new_data, data_, length_ * sizeof(T));
    } else {
      std::copy(&data_[0], &data_[length_], &new_data[0]);
    }
  }
  if (data_) zone->DeleteArray<T>(data_, capacity_);
  data_ = new_data;
  capacity_ = new_capacity;
}

template <typename T>
base::Vector<T> ZoneList<T>::AddBlock(T value, int count, Zone* zone) {
  int start = length_;
  for (int i = 0; i < count; i++) Add(value, zone);
  return base::Vector<T>(&data_[start], count);
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
void ZoneList<T>::Clear(Zone* zone) {
  if (data_) zone->DeleteArray<T>(data_, capacity_);
  DropAndClear();
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
  std::sort(begin(), end(),
            [cmp](const T& a, const T& b) { return cmp(&a, &b) < 0; });
#ifdef DEBUG
  for (int i = 1; i < length_; i++) {
    DCHECK_LE(cmp(&data_[i - 1], &data_[i]), 0);
  }
#endif
}

template <typename T>
template <typename CompareFunction>
void ZoneList<T>::StableSort(CompareFunction cmp, size_t s, size_t l) {
  std::stable_sort(begin() + s, begin() + s + l,
                   [cmp](const T& a, const T& b) { return cmp(&a, &b) < 0; });
#ifdef DEBUG
  for (size_t i = s + 1; i < l; i++) {
    DCHECK_LE(cmp(&data_[i - 1], &data_[i]), 0);
  }
#endif
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_LIST_INL_H_
