// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_HANDLE_SET_H_
#define V8_ZONE_ZONE_HANDLE_SET_H_

#include "src/handles.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

template <typename T>
class ZoneHandleSet final {
 public:
  ZoneHandleSet() : data_(kEmptyTag) {}
  explicit ZoneHandleSet(Handle<T> handle)
      : data_(handle.address() | kSingletonTag) {
    DCHECK(IsAligned(handle.address(), kPointerAlignment));
  }

  bool is_empty() const { return data_ == kEmptyTag; }

  size_t size() const {
    if ((data_ & kTagMask) == kEmptyTag) return 0;
    if ((data_ & kTagMask) == kSingletonTag) return 1;
    return list()->size();
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
    Address* const value = reinterpret_cast<Address*>(handle.address());
    DCHECK(IsAligned(reinterpret_cast<Address>(value), kPointerAlignment));
    if ((data_ & kTagMask) == kEmptyTag) {
      data_ = reinterpret_cast<Address>(value) | kSingletonTag;
    } else if ((data_ & kTagMask) == kSingletonTag) {
      if (singleton() == value) return;
      List* list = new (zone->New(sizeof(List))) List(zone);
      if (singleton() < value) {
        list->push_back(singleton());
        list->push_back(value);
      } else {
        list->push_back(value);
        list->push_back(singleton());
      }
      DCHECK(IsAligned(reinterpret_cast<Address>(list), kPointerAlignment));
      data_ = reinterpret_cast<Address>(list) | kListTag;
    } else {
      DCHECK_EQ(kListTag, data_ & kTagMask);
      List const* const old_list = list();
      for (size_t i = 0; i < old_list->size(); ++i) {
        if (old_list->at(i) == value) return;
        if (old_list->at(i) > value) break;
      }
      List* new_list = new (zone->New(sizeof(List))) List(zone);
      new_list->reserve(old_list->size() + 1);
      size_t i = 0;
      for (; i < old_list->size(); ++i) {
        if (old_list->at(i) > value) break;
        new_list->push_back(old_list->at(i));
      }
      new_list->push_back(value);
      for (; i < old_list->size(); ++i) {
        new_list->push_back(old_list->at(i));
      }
      DCHECK_EQ(old_list->size() + 1, new_list->size());
      DCHECK(IsAligned(reinterpret_cast<Address>(new_list), kPointerAlignment));
      data_ = reinterpret_cast<Address>(new_list) | kListTag;
    }
  }

  bool contains(ZoneHandleSet<T> const& other) const {
    if (data_ == other.data_) return true;
    if (data_ == kEmptyTag) return false;
    if (other.data_ == kEmptyTag) return true;
    if ((data_ & kTagMask) == kSingletonTag) return false;
    DCHECK_EQ(kListTag, data_ & kTagMask);
    List const* cached_list = list();
    if ((other.data_ & kTagMask) == kSingletonTag) {
      return std::find(cached_list->begin(), cached_list->end(),
                       other.singleton()) != cached_list->end();
    }
    DCHECK_EQ(kListTag, other.data_ & kTagMask);
    // TODO(bmeurer): Optimize this case.
    for (size_t i = 0; i < other.list()->size(); ++i) {
      if (std::find(cached_list->begin(), cached_list->end(),
                    other.list()->at(i)) == cached_list->end()) {
        return false;
      }
    }
    return true;
  }

  bool contains(Handle<T> other) const {
    if (data_ == kEmptyTag) return false;
    Address* other_address = reinterpret_cast<Address*>(other.address());
    if ((data_ & kTagMask) == kSingletonTag) {
      return singleton() == other_address;
    }
    DCHECK_EQ(kListTag, data_ & kTagMask);
    return std::find(list()->begin(), list()->end(), other_address) !=
           list()->end();
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
      if (lhs_list->size() == rhs_list->size()) {
        for (size_t i = 0; i < lhs_list->size(); ++i) {
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

  class const_iterator;
  inline const_iterator begin() const;
  inline const_iterator end() const;

 private:
  typedef ZoneVector<Address*> List;

  List const* list() const {
    DCHECK_EQ(kListTag, data_ & kTagMask);
    return reinterpret_cast<List const*>(data_ - kListTag);
  }

  Address* singleton() const {
    DCHECK_EQ(kSingletonTag, data_ & kTagMask);
    return reinterpret_cast<Address*>(data_ - kSingletonTag);
  }

  enum Tag : Address {
    kSingletonTag = 0,
    kEmptyTag = 1,
    kListTag = 2,
    kTagMask = 3
  };

  STATIC_ASSERT(kTagMask < kPointerAlignment);

  Address data_;
};

template <typename T>
std::ostream& operator<<(std::ostream& os, ZoneHandleSet<T> set) {
  for (size_t i = 0; i < set.size(); ++i) {
    if (i > 0) os << ", ";
    os << set.at(i);
  }
  return os;
}

template <typename T>
class ZoneHandleSet<T>::const_iterator {
 public:
  typedef std::forward_iterator_tag iterator_category;
  typedef std::ptrdiff_t difference_type;
  typedef Handle<T> value_type;
  typedef value_type reference;
  typedef value_type* pointer;

  const_iterator(const const_iterator& other)
      : set_(other.set_), current_(other.current_) {}

  reference operator*() const { return (*set_)[current_]; }
  bool operator==(const const_iterator& other) const {
    return set_ == other.set_ && current_ == other.current_;
  }
  bool operator!=(const const_iterator& other) const {
    return !(*this == other);
  }
  const_iterator& operator++() {
    DCHECK(current_ < set_->size());
    current_ += 1;
    return *this;
  }
  const_iterator operator++(int);

 private:
  friend class ZoneHandleSet<T>;

  explicit const_iterator(const ZoneHandleSet<T>* set, size_t current)
      : set_(set), current_(current) {}

  const ZoneHandleSet<T>* set_;
  size_t current_;
};

template <typename T>
typename ZoneHandleSet<T>::const_iterator ZoneHandleSet<T>::begin() const {
  return ZoneHandleSet<T>::const_iterator(this, 0);
}

template <typename T>
typename ZoneHandleSet<T>::const_iterator ZoneHandleSet<T>::end() const {
  return ZoneHandleSet<T>::const_iterator(this, size());
}

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_HANDLE_SET_H_
