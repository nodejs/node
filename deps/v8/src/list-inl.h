// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIST_INL_H_
#define V8_LIST_INL_H_

#include "src/list.h"

#include "src/base/macros.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace internal {


template<typename T, class P>
void List<T, P>::Add(const T& element, P alloc) {
  if (length_ < capacity_) {
    data_[length_++] = element;
  } else {
    List<T, P>::ResizeAdd(element, alloc);
  }
}


template<typename T, class P>
void List<T, P>::AddAll(const List<T, P>& other, P alloc) {
  AddAll(other.ToVector(), alloc);
}


template<typename T, class P>
void List<T, P>::AddAll(const Vector<T>& other, P alloc) {
  int result_length = length_ + other.length();
  if (capacity_ < result_length) Resize(result_length, alloc);
  if (base::is_fundamental<T>()) {
    memcpy(data_ + length_, other.start(), sizeof(*data_) * other.length());
  } else {
    for (int i = 0; i < other.length(); i++) data_[length_ + i] = other.at(i);
  }
  length_ = result_length;
}


// Use two layers of inlining so that the non-inlined function can
// use the same implementation as the inlined version.
template<typename T, class P>
void List<T, P>::ResizeAdd(const T& element, P alloc) {
  ResizeAddInternal(element, alloc);
}


template<typename T, class P>
void List<T, P>::ResizeAddInternal(const T& element, P alloc) {
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


template<typename T, class P>
void List<T, P>::Resize(int new_capacity, P alloc) {
  DCHECK_LE(length_, new_capacity);
  T* new_data = NewData(new_capacity, alloc);
  MemCopy(new_data, data_, length_ * sizeof(T));
  List<T, P>::DeleteData(data_);
  data_ = new_data;
  capacity_ = new_capacity;
}


template<typename T, class P>
Vector<T> List<T, P>::AddBlock(T value, int count, P alloc) {
  int start = length_;
  for (int i = 0; i < count; i++) Add(value, alloc);
  return Vector<T>(&data_[start], count);
}


template<typename T, class P>
void List<T, P>::Set(int index, const T& elm) {
  DCHECK(index >= 0 && index <= length_);
  data_[index] = elm;
}


template<typename T, class P>
void List<T, P>::InsertAt(int index, const T& elm, P alloc) {
  DCHECK(index >= 0 && index <= length_);
  Add(elm, alloc);
  for (int i = length_ - 1; i > index; --i) {
    data_[i] = data_[i - 1];
  }
  data_[index] = elm;
}


template<typename T, class P>
T List<T, P>::Remove(int i) {
  T element = at(i);
  length_--;
  while (i < length_) {
    data_[i] = data_[i + 1];
    i++;
  }
  return element;
}


template<typename T, class P>
bool List<T, P>::RemoveElement(const T& elm) {
  for (int i = 0; i < length_; i++) {
    if (data_[i] == elm) {
      Remove(i);
      return true;
    }
  }
  return false;
}

template <typename T, class P>
void List<T, P>::Swap(List<T, P>* list) {
  std::swap(data_, list->data_);
  std::swap(length_, list->length_);
  std::swap(capacity_, list->capacity_);
}

template<typename T, class P>
void List<T, P>::Allocate(int length, P allocator) {
  DeleteData(data_);
  Initialize(length, allocator);
  length_ = length;
}


template<typename T, class P>
void List<T, P>::Clear() {
  DeleteData(data_);
  // We don't call Initialize(0) since that requires passing a Zone,
  // which we don't really need.
  data_ = NULL;
  capacity_ = 0;
  length_ = 0;
}


template<typename T, class P>
void List<T, P>::Rewind(int pos) {
  DCHECK(0 <= pos && pos <= length_);
  length_ = pos;
}


template<typename T, class P>
void List<T, P>::Trim(P alloc) {
  if (length_ < capacity_ / 4) {
    Resize(capacity_ / 2, alloc);
  }
}


template<typename T, class P>
void List<T, P>::Iterate(void (*callback)(T* x)) {
  for (int i = 0; i < length_; i++) callback(&data_[i]);
}


template<typename T, class P>
template<class Visitor>
void List<T, P>::Iterate(Visitor* visitor) {
  for (int i = 0; i < length_; i++) visitor->Apply(&data_[i]);
}


template<typename T, class P>
bool List<T, P>::Contains(const T& elm) const {
  for (int i = 0; i < length_; i++) {
    if (data_[i] == elm)
      return true;
  }
  return false;
}


template<typename T, class P>
int List<T, P>::CountOccurrences(const T& elm, int start, int end) const {
  int result = 0;
  for (int i = start; i <= end; i++) {
    if (data_[i] == elm) ++result;
  }
  return result;
}


template <typename T, class P>
template <typename CompareFunction>
void List<T, P>::Sort(CompareFunction cmp) {
  Sort(cmp, 0, length_);
}


template <typename T, class P>
template <typename CompareFunction>
void List<T, P>::Sort(CompareFunction cmp, size_t s, size_t l) {
  ToVector().Sort(cmp, s, l);
#ifdef DEBUG
  for (size_t i = s + 1; i < l; i++) DCHECK(cmp(&data_[i - 1], &data_[i]) <= 0);
#endif
}


template<typename T, class P>
void List<T, P>::Sort() {
  ToVector().Sort();
}


template <typename T, class P>
template <typename CompareFunction>
void List<T, P>::StableSort(CompareFunction cmp) {
  StableSort(cmp, 0, length_);
}


template <typename T, class P>
template <typename CompareFunction>
void List<T, P>::StableSort(CompareFunction cmp, size_t s, size_t l) {
  ToVector().StableSort(cmp, s, l);
#ifdef DEBUG
  for (size_t i = s + 1; i < l; i++) DCHECK(cmp(&data_[i - 1], &data_[i]) <= 0);
#endif
}


template <typename T, class P>
void List<T, P>::StableSort() {
  ToVector().StableSort();
}


template <typename T, typename P>
int SortedListBSearch(const List<T>& list, P cmp) {
  int low = 0;
  int high = list.length() - 1;
  while (low <= high) {
    int mid = low + (high - low) / 2;
    T mid_elem = list[mid];

    if (cmp(&mid_elem) > 0) {
      high = mid - 1;
      continue;
    }
    if (cmp(&mid_elem) < 0) {
      low = mid + 1;
      continue;
    }
    // Found the elememt.
    return mid;
  }
  return -1;
}


template<typename T>
class ElementCmp {
 public:
  explicit ElementCmp(T e) : elem_(e) {}
  int operator()(const T* other) {
    return PointerValueCompare(other, &elem_);
  }
 private:
  T elem_;
};


template <typename T>
int SortedListBSearch(const List<T>& list, T elem) {
  return SortedListBSearch<T, ElementCmp<T> > (list, ElementCmp<T>(elem));
}


}  // namespace internal
}  // namespace v8

#endif  // V8_LIST_INL_H_
