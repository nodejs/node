// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SMALL_POINTER_LIST_H_
#define V8_SMALL_POINTER_LIST_H_

#include "src/base/logging.h"
#include "src/globals.h"
#include "src/zone.h"

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

  SmallPointerList(int capacity, Zone* zone) : data_(kEmptyTag) {
    Reserve(capacity, zone);
  }

  void Reserve(int capacity, Zone* zone) {
    if (capacity < 2) return;
    if ((data_ & kTagMask) == kListTag) {
      if (list()->capacity() >= capacity) return;
      int old_length = list()->length();
      list()->AddBlock(NULL, capacity - list()->capacity(), zone);
      list()->Rewind(old_length);
      return;
    }
    PointerList* list = new(zone) PointerList(capacity, zone);
    if ((data_ & kTagMask) == kSingletonTag) {
      list->Add(single_value(), zone);
    }
    DCHECK(IsAligned(reinterpret_cast<intptr_t>(list), kPointerAlignment));
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

  void Add(T* pointer, Zone* zone) {
    DCHECK(IsAligned(reinterpret_cast<intptr_t>(pointer), kPointerAlignment));
    if ((data_ & kTagMask) == kEmptyTag) {
      data_ = reinterpret_cast<intptr_t>(pointer) | kSingletonTag;
      return;
    }
    if ((data_ & kTagMask) == kSingletonTag) {
      PointerList* list = new(zone) PointerList(2, zone);
      list->Add(single_value(), zone);
      list->Add(pointer, zone);
      DCHECK(IsAligned(reinterpret_cast<intptr_t>(list), kPointerAlignment));
      data_ = reinterpret_cast<intptr_t>(list) | kListTag;
      return;
    }
    list()->Add(pointer, zone);
  }

  // Note: returns T* and not T*& (unlike List from list.h).
  // This makes the implementation simpler and more const correct.
  T* at(int i) const {
    DCHECK((data_ & kTagMask) != kEmptyTag);
    if ((data_ & kTagMask) == kSingletonTag) {
      DCHECK(i == 0);
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
    DCHECK((data_ & kTagMask) != kEmptyTag);
    if ((data_ & kTagMask) == kSingletonTag) {
      T* result = single_value();
      data_ = kEmptyTag;
      return result;
    }
    return list()->RemoveLast();
  }

  void Rewind(int pos) {
    if ((data_ & kTagMask) == kEmptyTag) {
      DCHECK(pos == 0);
      return;
    }
    if ((data_ & kTagMask) == kSingletonTag) {
      DCHECK(pos == 0 || pos == 1);
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
    DCHECK((data_ & kTagMask) == kSingletonTag);
    STATIC_ASSERT(kSingletonTag == 0);
    return reinterpret_cast<T*>(data_);
  }

  PointerList* list() const {
    DCHECK((data_ & kTagMask) == kListTag);
    return reinterpret_cast<PointerList*>(data_ & kValueMask);
  }

  intptr_t data_;

  DISALLOW_COPY_AND_ASSIGN(SmallPointerList);
};

} }  // namespace v8::internal

#endif  // V8_SMALL_POINTER_LIST_H_
