// Copyright 2011 the V8 project authors. All rights reserved.
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

#ifndef V8_SMALL_POINTER_LIST_H_
#define V8_SMALL_POINTER_LIST_H_

#include "checks.h"
#include "v8globals.h"
#include "zone.h"

namespace v8 {
namespace internal {

// SmallPointerList is a list optimized for storing no or just a
// single value. When more values are given it falls back to ZoneList.
//
// The interface tries to be as close to List from list.h as possible.
template <typename T>
class SmallPointerList {
 public:
  SmallPointerList() : data_(kEmptyTag) {}

  explicit SmallPointerList(int capacity) : data_(kEmptyTag) {
    Reserve(capacity);
  }

  void Reserve(int capacity) {
    if (capacity < 2) return;
    if ((data_ & kTagMask) == kListTag) {
      if (list()->capacity() >= capacity) return;
      int old_length = list()->length();
      list()->AddBlock(NULL, capacity - list()->capacity());
      list()->Rewind(old_length);
      return;
    }
    PointerList* list = new PointerList(capacity);
    if ((data_ & kTagMask) == kSingletonTag) {
      list->Add(single_value());
    }
    ASSERT(IsAligned(reinterpret_cast<intptr_t>(list), kPointerAlignment));
    data_ = reinterpret_cast<intptr_t>(list) | kListTag;
  }

  void Clear() {
    data_ = kEmptyTag;
  }

  void Sort() {
    if ((data_ & kTagMask) == kListTag) {
      list()->Sort(compare_value);
    }
  }

  bool is_empty() const { return length() == 0; }

  int length() const {
    if ((data_ & kTagMask) == kEmptyTag) return 0;
    if ((data_ & kTagMask) == kSingletonTag) return 1;
    return list()->length();
  }

  void Add(T* pointer) {
    ASSERT(IsAligned(reinterpret_cast<intptr_t>(pointer), kPointerAlignment));
    if ((data_ & kTagMask) == kEmptyTag) {
      data_ = reinterpret_cast<intptr_t>(pointer) | kSingletonTag;
      return;
    }
    if ((data_ & kTagMask) == kSingletonTag) {
      PointerList* list = new PointerList(2);
      list->Add(single_value());
      list->Add(pointer);
      ASSERT(IsAligned(reinterpret_cast<intptr_t>(list), kPointerAlignment));
      data_ = reinterpret_cast<intptr_t>(list) | kListTag;
      return;
    }
    list()->Add(pointer);
  }

  // Note: returns T* and not T*& (unlike List from list.h).
  // This makes the implementation simpler and more const correct.
  T* at(int i) const {
    ASSERT((data_ & kTagMask) != kEmptyTag);
    if ((data_ & kTagMask) == kSingletonTag) {
      ASSERT(i == 0);
      return single_value();
    }
    return list()->at(i);
  }

  // See the note above.
  T* operator[](int i) const { return at(i); }

  // Remove the given element from the list (if present).
  void RemoveElement(T* pointer) {
    if ((data_ & kTagMask) == kEmptyTag) return;
    if ((data_ & kTagMask) == kSingletonTag) {
      if (pointer == single_value()) {
        data_ = kEmptyTag;
      }
      return;
    }
    list()->RemoveElement(pointer);
  }

  T* RemoveLast() {
    ASSERT((data_ & kTagMask) != kEmptyTag);
    if ((data_ & kTagMask) == kSingletonTag) {
      T* result = single_value();
      data_ = kEmptyTag;
      return result;
    }
    return list()->RemoveLast();
  }

  void Rewind(int pos) {
    if ((data_ & kTagMask) == kEmptyTag) {
      ASSERT(pos == 0);
      return;
    }
    if ((data_ & kTagMask) == kSingletonTag) {
      ASSERT(pos == 0 || pos == 1);
      if (pos == 0) {
        data_ = kEmptyTag;
      }
      return;
    }
    list()->Rewind(pos);
  }

  int CountOccurrences(T* pointer, int start, int end) const {
    if ((data_ & kTagMask) == kEmptyTag) return 0;
    if ((data_ & kTagMask) == kSingletonTag) {
      if (start == 0 && end >= 0) {
        return (single_value() == pointer) ? 1 : 0;
      }
      return 0;
    }
    return list()->CountOccurrences(pointer, start, end);
  }

 private:
  typedef ZoneList<T*> PointerList;

  static int compare_value(T* const* a, T* const* b) {
    return Compare<T>(**a, **b);
  }

  static const intptr_t kEmptyTag = 1;
  static const intptr_t kSingletonTag = 0;
  static const intptr_t kListTag = 2;
  static const intptr_t kTagMask = 3;
  static const intptr_t kValueMask = ~kTagMask;

  STATIC_ASSERT(kTagMask + 1 <= kPointerAlignment);

  T* single_value() const {
    ASSERT((data_ & kTagMask) == kSingletonTag);
    STATIC_ASSERT(kSingletonTag == 0);
    return reinterpret_cast<T*>(data_);
  }

  PointerList* list() const {
    ASSERT((data_ & kTagMask) == kListTag);
    return reinterpret_cast<PointerList*>(data_ & kValueMask);
  }

  intptr_t data_;

  DISALLOW_COPY_AND_ASSIGN(SmallPointerList);
};

} }  // namespace v8::internal

#endif  // V8_SMALL_POINTER_LIST_H_
