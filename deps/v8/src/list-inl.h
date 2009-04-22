// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

namespace v8 { namespace internal {


template<typename T, class P>
void List<T, P>::Add(const T& element) {
  if (length_ < capacity_) {
    data_[length_++] = element;
  } else {
    // Grow the list capacity by 50%, but make sure to let it grow
    // even when the capacity is zero (possible initial case).
    int new_capacity = 1 + capacity_ + (capacity_ >> 1);
    T* new_data = NewData(new_capacity);
    memcpy(new_data, data_, capacity_ * sizeof(T));
    // Since the element reference could be an element of the list,
    // assign it to the new backing store before deleting the old.
    new_data[length_++] = element;
    DeleteData(data_);
    data_ = new_data;
    capacity_ = new_capacity;
  }
}


template<typename T, class P>
Vector<T> List<T, P>::AddBlock(T value, int count) {
  int start = length_;
  for (int i = 0; i < count; i++) Add(value);
  return Vector<T>(&data_[start], count);
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
void List<T, P>::Clear() {
  DeleteData(data_);
  Initialize(0);
}


template<typename T, class P>
void List<T, P>::Rewind(int pos) {
  length_ = pos;
}


template<typename T, class P>
void List<T, P>::Iterate(void (*callback)(T* x)) {
  for (int i = 0; i < length_; i++) callback(&data_[i]);
}


template<typename T, class P>
bool List<T, P>::Contains(const T& elm) {
  for (int i = 0; i < length_; i++) {
    if (data_[i] == elm)
      return true;
  }
  return false;
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
void List<T, P>::Initialize(int capacity) {
  ASSERT(capacity >= 0);
  data_ = (capacity > 0) ? NewData(capacity) : NULL;
  capacity_ = capacity;
  length_ = 0;
}


} }  // namespace v8::internal

#endif  // V8_LIST_INL_H_
