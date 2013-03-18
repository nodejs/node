// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_LIST_INL_H_
#define V8_LIST_INL_H_

#include "list.h"

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
  for (int i = 0; i < other.length(); i++) {
    data_[length_ + i] = other.at(i);
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
  ASSERT(length_ >= capacity_);
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
  ASSERT_LE(length_, new_capacity);
  T* new_data = NewData(new_capacity, alloc);
  memcpy(new_data, data_, length_ * sizeof(T));
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
void List<T, P>::InsertAt(int index, const T& elm, P alloc) {
  ASSERT(index >= 0 && index <= length_);
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


template<typename T, class P>
void List<T, P>::Sort(int (*cmp)(const T* x, const T* y)) {
  ToVector().Sort(cmp);
#ifdef DEBUG
  for (int i = 1; i < length_; i++)
    ASSERT(cmp(&data_[i - 1], &data_[i]) <= 0);
#endif
}


template<typename T, class P>
void List<T, P>::Sort() {
  Sort(PointerValueCompare<T>);
}


template<typename T, class P>
void List<T, P>::Initialize(int capacity, P allocator) {
  ASSERT(capacity >= 0);
  data_ = (capacity > 0) ? NewData(capacity, allocator) : NULL;
  capacity_ = capacity;
  length_ = 0;
}


template <typename T, typename P>
int SortedListBSearch(const List<T>& list, P cmp) {
  int low = 0;
  int high = list.length() - 1;
  while (low <= high) {
    int mid = (low + high) / 2;
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


} }  // namespace v8::internal

#endif  // V8_LIST_INL_H_
