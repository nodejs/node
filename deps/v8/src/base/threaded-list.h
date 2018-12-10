// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_THREADED_LIST_H_
#define V8_BASE_THREADED_LIST_H_

#include <iterator>

#include "src/base/compiler-specific.h"
#include "src/base/macros.h"

namespace v8 {
namespace base {

template <typename T>
struct ThreadedListTraits {
  static T** next(T* t) { return t->next(); }
  static T** start(T** t) { return t; }
  static T* const* start(T* const* t) { return t; }
};

// Represents a linked list that threads through the nodes in the linked list.
// Entries in the list are pointers to nodes. By default nodes need to have a
// T** next() method that returns the location where the next value is stored.
// The default can be overwritten by providing a ThreadedTraits class.
template <typename T, typename BaseClass,
          typename TLTraits = ThreadedListTraits<T>>
class ThreadedListBase final : public BaseClass {
 public:
  ThreadedListBase() : head_(nullptr), tail_(&head_) {}
  void Add(T* v) {
    DCHECK_NULL(*tail_);
    DCHECK_NULL(*TLTraits::next(v));
    *tail_ = v;
    tail_ = TLTraits::next(v);
  }

  void AddFront(T* v) {
    DCHECK_NULL(*TLTraits::next(v));
    DCHECK_NOT_NULL(v);
    T** const next = TLTraits::next(v);

    *next = head_;
    if (head_ == nullptr) tail_ = next;
    head_ = v;
  }

  void DropHead() {
    DCHECK_NOT_NULL(head_);

    T* old_head = head_;
    head_ = *TLTraits::next(head_);
    if (head_ == nullptr) tail_ = &head_;
    *TLTraits::next(old_head) = nullptr;
  }

  bool Contains(T* v) {
    for (Iterator it = begin(); it != end(); ++it) {
      if (*it == v) return true;
    }
    return false;
  }

  void Append(ThreadedListBase&& list) {
    if (list.is_empty()) return;

    *tail_ = list.head_;
    tail_ = list.tail_;
    list.Clear();
  }

  void Prepend(ThreadedListBase&& list) {
    if (list.head_ == nullptr) return;

    T* new_head = list.head_;
    *list.tail_ = head_;
    if (head_ == nullptr) {
      tail_ = list.tail_;
    }
    head_ = new_head;
    list.Clear();
  }

  void Clear() {
    head_ = nullptr;
    tail_ = &head_;
  }

  ThreadedListBase& operator=(ThreadedListBase&& other) V8_NOEXCEPT {
    head_ = other.head_;
    tail_ = other.head_ ? other.tail_ : &head_;
#ifdef DEBUG
    other.Clear();
#endif
    return *this;
  }

  ThreadedListBase(ThreadedListBase&& other) V8_NOEXCEPT
      : head_(other.head_),
        tail_(other.head_ ? other.tail_ : &head_) {
#ifdef DEBUG
    other.Clear();
#endif
  }

  bool Remove(T* v) {
    T* current = first();
    if (current == v) {
      DropHead();
      return true;
    }

    while (current != nullptr) {
      T* next = *TLTraits::next(current);
      if (next == v) {
        *TLTraits::next(current) = *TLTraits::next(next);
        *TLTraits::next(next) = nullptr;

        if (TLTraits::next(next) == tail_) {
          tail_ = TLTraits::next(current);
        }
        return true;
      }
      current = next;
    }
    return false;
  }

  class Iterator final {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T*;
    using reference = value_type;
    using pointer = value_type*;

   public:
    Iterator& operator++() {
      entry_ = TLTraits::next(*entry_);
      return *this;
    }
    bool operator==(const Iterator& other) const {
      return entry_ == other.entry_;
    }
    bool operator!=(const Iterator& other) const {
      return entry_ != other.entry_;
    }
    T*& operator*() { return *entry_; }
    T* operator->() { return *entry_; }
    Iterator& operator=(T* entry) {
      T* next = *TLTraits::next(*entry_);
      *TLTraits::next(entry) = next;
      *entry_ = entry;
      return *this;
    }

    Iterator() : entry_(nullptr) {}

   private:
    explicit Iterator(T** entry) : entry_(entry) {}

    T** entry_;

    friend class ThreadedListBase;
  };

  class ConstIterator final {
   public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T*;
    using reference = const value_type;
    using pointer = const value_type*;

   public:
    ConstIterator& operator++() {
      entry_ = TLTraits::next(*entry_);
      return *this;
    }
    bool operator==(const ConstIterator& other) const {
      return entry_ == other.entry_;
    }
    bool operator!=(const ConstIterator& other) const {
      return entry_ != other.entry_;
    }
    const T* operator*() const { return *entry_; }

   private:
    explicit ConstIterator(T* const* entry) : entry_(entry) {}

    T* const* entry_;

    friend class ThreadedListBase;
  };

  Iterator begin() { return Iterator(TLTraits::start(&head_)); }
  Iterator end() { return Iterator(tail_); }

  ConstIterator begin() const { return ConstIterator(TLTraits::start(&head_)); }
  ConstIterator end() const { return ConstIterator(tail_); }

  // Rewinds the list's tail to the reset point, i.e., cutting of the rest of
  // the list, including the reset_point.
  void Rewind(Iterator reset_point) {
    tail_ = reset_point.entry_;
    *tail_ = nullptr;
  }

  // Moves the tail of the from_list, starting at the from_location, to the end
  // of this list.
  void MoveTail(ThreadedListBase* from_list, Iterator from_location) {
    if (from_list->end() != from_location) {
      DCHECK_NULL(*tail_);
      *tail_ = *from_location;
      tail_ = from_list->tail_;
      from_list->Rewind(from_location);
    }
  }

  bool is_empty() const { return head_ == nullptr; }

  T* first() const { return head_; }

  // Slow. For testing purposes.
  int LengthForTest() {
    int result = 0;
    for (Iterator t = begin(); t != end(); ++t) ++result;
    return result;
  }

  T* AtForTest(int i) {
    Iterator t = begin();
    while (i-- > 0) ++t;
    return *t;
  }

  bool Verify() {
    T* last = this->first();
    if (last == nullptr) {
      CHECK_EQ(&head_, tail_);
    } else {
      while (*TLTraits::next(last) != nullptr) {
        last = *TLTraits::next(last);
      }
      CHECK_EQ(TLTraits::next(last), tail_);
    }
    return true;
  }

 private:
  T* head_;
  T** tail_;
  DISALLOW_COPY_AND_ASSIGN(ThreadedListBase);
};

struct EmptyBase {};

template <typename T, typename TLTraits = ThreadedListTraits<T>>
using ThreadedList = ThreadedListBase<T, EmptyBase, TLTraits>;

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_THREADED_LIST_H_
