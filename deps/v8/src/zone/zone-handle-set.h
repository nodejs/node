// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_HANDLE_SET_H_
#define V8_ZONE_ZONE_HANDLE_SET_H_

#include "src/handles.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

template <typename T>
class ZoneHandleSet final {
 public:
  ZoneHandleSet() : data_(kEmptyTag) {}
  explicit ZoneHandleSet(Handle<T> handle)
      : data_(bit_cast<intptr_t>(handle.address()) | kSingletonTag) {
    DCHECK(IsAligned(bit_cast<intptr_t>(handle.address()), kPointerAlignment));
  }

  bool is_empty() const { return data_ == kEmptyTag; }

  size_t size() const {
    if ((data_ & kTagMask) == kEmptyTag) return 0;
    if ((data_ & kTagMask) == kSingletonTag) return 1;
    return list()->length();
  }

  Handle<T> at(size_t i) const {
    DCHECK_NE(kEmptyTag, data_ & kTagMask);
    if ((data_ & kTagMask) == kSingletonTag) {
      DCHECK_EQ(0u, i);
      return Handle<T>(singleton());
    }
    return Handle<T>(list()->at(static_cast<int>(i)));
  }

  Handle<T> operator[](size_t i) const { return at(i); }

  void insert(Handle<T> handle, Zone* zone) {
    T** const value = bit_cast<T**>(handle.address());
    DCHECK(IsAligned(bit_cast<intptr_t>(value), kPointerAlignment));
    if ((data_ & kTagMask) == kEmptyTag) {
      data_ = bit_cast<intptr_t>(value) | kSingletonTag;
    } else if ((data_ & kTagMask) == kSingletonTag) {
      if (singleton() == value) return;
      List* list = new (zone) List(2, zone);
      if (singleton() < value) {
        list->Add(singleton(), zone);
        list->Add(value, zone);
      } else {
        list->Add(value, zone);
        list->Add(singleton(), zone);
      }
      DCHECK(IsAligned(bit_cast<intptr_t>(list), kPointerAlignment));
      data_ = bit_cast<intptr_t>(list) | kListTag;
    } else {
      DCHECK_EQ(kListTag, data_ & kTagMask);
      List const* const old_list = list();
      for (int i = 0; i < old_list->length(); ++i) {
        if (old_list->at(i) == value) return;
        if (old_list->at(i) > value) break;
      }
      List* new_list = new (zone) List(old_list->length() + 1, zone);
      int i = 0;
      for (; i < old_list->length(); ++i) {
        if (old_list->at(i) > value) break;
        new_list->Add(old_list->at(i), zone);
      }
      new_list->Add(value, zone);
      for (; i < old_list->length(); ++i) {
        new_list->Add(old_list->at(i), zone);
      }
      DCHECK_EQ(old_list->length() + 1, new_list->length());
      DCHECK(IsAligned(bit_cast<intptr_t>(new_list), kPointerAlignment));
      data_ = bit_cast<intptr_t>(new_list) | kListTag;
    }
  }

  bool contains(ZoneHandleSet<T> const& other) const {
    if (data_ == other.data_) return true;
    if (data_ == kEmptyTag) return false;
    if (other.data_ == kEmptyTag) return true;
    if ((data_ & kTagMask) == kSingletonTag) return false;
    DCHECK_EQ(kListTag, data_ & kTagMask);
    if ((other.data_ & kTagMask) == kSingletonTag) {
      return list()->Contains(other.singleton());
    }
    DCHECK_EQ(kListTag, other.data_ & kTagMask);
    // TODO(bmeurer): Optimize this case.
    for (int i = 0; i < other.list()->length(); ++i) {
      if (!list()->Contains(other.list()->at(i))) return false;
    }
    return true;
  }

  void remove(Handle<T> handle, Zone* zone) {
    // TODO(bmeurer): Optimize this case.
    ZoneHandleSet<T> that;
    for (size_t i = 0; i < size(); ++i) {
      Handle<T> value = at(i);
      if (value.address() != handle.address()) {
        that.insert(value, zone);
      }
    }
    std::swap(*this, that);
  }

  friend bool operator==(ZoneHandleSet<T> const& lhs,
                         ZoneHandleSet<T> const& rhs) {
    if (lhs.data_ == rhs.data_) return true;
    if ((lhs.data_ & kTagMask) == kListTag &&
        (rhs.data_ & kTagMask) == kListTag) {
      List const* const lhs_list = lhs.list();
      List const* const rhs_list = rhs.list();
      if (lhs_list->length() == rhs_list->length()) {
        for (int i = 0; i < lhs_list->length(); ++i) {
          if (lhs_list->at(i) != rhs_list->at(i)) return false;
        }
        return true;
      }
    }
    return false;
  }

  friend bool operator!=(ZoneHandleSet<T> const& lhs,
                         ZoneHandleSet<T> const& rhs) {
    return !(lhs == rhs);
  }

  friend size_t hash_value(ZoneHandleSet<T> const& set) {
    return static_cast<size_t>(set.data_);
  }

 private:
  typedef ZoneList<T**> List;

  List const* list() const {
    DCHECK_EQ(kListTag, data_ & kTagMask);
    return bit_cast<List const*>(data_ - kListTag);
  }

  T** singleton() const {
    DCHECK_EQ(kSingletonTag, data_ & kTagMask);
    return bit_cast<T**>(data_ - kSingletonTag);
  }

  enum Tag : intptr_t {
    kSingletonTag = 0,
    kEmptyTag = 1,
    kListTag = 2,
    kTagMask = 3
  };

  STATIC_ASSERT(kTagMask < kPointerAlignment);

  intptr_t data_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_HANDLE_SET_H_
