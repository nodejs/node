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
// The kSupportsUnsafeInsertion flag defines whether the list supports insertion
// of new elements into the list by just rewiring the next pointers without
// updating the list object itself. Such an insertion might invalidate the
// pointer to list tail and thus requires additional steps to recover the
// pointer to the tail.
// The default can be overwritten by providing a ThreadedTraits class.
template <typename T, typename BaseClass,
          typename TLTraits = ThreadedListTraits<T>,
          bool kSupportsUnsafeInsertion = false>
class ThreadedListBase final : public BaseClass {
 public:
  ThreadedListBase() : head_(nullptr), tail_(&head_) {}
  ThreadedListBase(const ThreadedListBase&) = delete;
  ThreadedListBase& operator=(const ThreadedListBase&) = delete;

  void Add(T* v) {
    EnsureValidTail();
    DCHECK_NULL(*tail_);
    DCHECK_NULL(*TLTraits::next(v));
    *tail_ = v;
    tail_ = TLTraits::next(v);
    // Check that only one element was added (and that hasn't created a cycle).
    DCHECK_NULL(*tail_);
  }

  void AddFront(T* v) {
    DCHECK_NULL(*TLTraits::next(v));
    DCHECK_NOT_NULL(v);
    T** const next = TLTraits::next(v);

    *next = head_;
    if (head_ == nullptr) tail_ = next;
    head_ = v;
  }

  // This temporarily breaks the tail_ invariant, and it should only be called
  // if we support unsafe insertions.
  static void AddAfter(T* after_this, T* v) {
    DCHECK(kSupportsUnsafeInsertion);
    DCHECK_NULL(*TLTraits::next(v));
    *TLTraits::next(v) = *TLTraits::next(after_this);
    *TLTraits::next(after_this) = v;
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

    EnsureValidTail();
    *tail_ = list.head_;
    tail_ = list.tail_;
    list.Clear();
  }

  void Prepend(ThreadedListBase&& list) {
    if (list.head_ == nullptr) return;

    EnsureValidTail();
    T* new_head = list.head_;
    *list.tail_ = head_;
    if (head_ == nullptr) {
      tail_ = list.tail_;
    }
    head_ = new_head;
    list.Clear();
  }

  // This is only valid if {v} is in the current list.
  void TruncateAt(ThreadedListBase* rem, T* v) {
    CHECK_NOT_NULL(rem);
    CHECK_NOT_NULL(v);
    CHECK(rem->is_empty());
    Iterator it = begin();
    T* last = nullptr;
    for (; it != end(); ++it) {
      if (*it == v) {
        break;
      }
      last = *it;
    }
    CHECK_EQ(v, *it);

    // Remaining list.
    rem->head_ = v;
    rem->tail_ = tail_;

    if (last == nullptr) {
      // The head must point to v, so we return the empty list.
      CHECK_EQ(head_, v);
      Clear();
    } else {
      tail_ = TLTraits::next(last);
      *tail_ = nullptr;
    }
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

    EnsureValidTail();
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
    T*& operator*() { return *entry_; }
    T* operator->() { return *entry_; }
    Iterator& operator=(T* entry) {
      T* next = *TLTraits::next(*entry_);
      *TLTraits::next(entry) = next;
      *entry_ = entry;
      return *this;
    }

    bool is_null() { return entry_ == nullptr; }

    void InsertBefore(T* value) {
      T* old_entry_value = *entry_;
      *entry_ = value;
      entry_ = TLTraits::next(value);
      *entry_ = old_entry_value;
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

    // Allow implicit conversion to const iterator.
    // NOLINTNEXTLINE
    ConstIterator(Iterator& iterator) : entry_(iterator.entry_) {}

   public:
    ConstIterator& operator++() {
      entry_ = TLTraits::next(*entry_);
      return *this;
    }
    bool operator==(const ConstIterator& other) const {
      return entry_ == other.entry_;
    }
    const T* operator*() const { return *entry_; }

   private:
    explicit ConstIterator(T* const* entry) : entry_(entry) {}

    T* const* entry_;

    friend class ThreadedListBase;
  };

  Iterator begin() { return Iterator(TLTraits::start(&head_)); }
  Iterator end() {
    EnsureValidTail();
    return Iterator(tail_);
  }

  ConstIterator begin() const { return ConstIterator(TLTraits::start(&head_)); }
  ConstIterator end() const {
    EnsureValidTail();
    return ConstIterator(tail_);
  }

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

  // Removes the element at `it`, and returns a new iterator pointing to the
  // element following the removed element (if `it` was pointing to the last
  // element, then `end()` is returned). The head and the tail are updated. `it`
  // should not be `end()`. Iterators that are currently on the same element as
  // `it` are invalidated. Other iterators are not affected. If the last element
  // is removed, existing `end()` iterators will be invalidated.
  Iterator RemoveAt(Iterator it) {
    if (*it.entry_ == head_) {
      DropHead();
      return begin();
    } else if (tail_ == TLTraits::next(*it.entry_)) {
      tail_ = it.entry_;
      *it.entry_ = nullptr;
      return end();
    } else {
      T* old_entry = *it.entry_;
      *it.entry_ = *TLTraits::next(*it.entry_);
      *TLTraits::next(old_entry) = nullptr;
      return Iterator(it.entry_);
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

  bool Verify() const {
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

  inline void EnsureValidTail() const {
    if (!kSupportsUnsafeInsertion) {
      DCHECK_EQ(*tail_, nullptr);
      return;
    }
    // If kSupportsUnsafeInsertion, then we support adding a new element by
    // using the pointer to a certain element. E.g., imagine list A -> B -> C,
    // we can add D after B, by just moving the pointer of B to D and D to
    // whatever B used to point to. We do not need to know the beginning of the
    // list (ie. to have a pointer to the ThreadList class). This however might
    // break the tail_ invariant. We ensure this here, by manually looking for
    // the tail of the list.
    if (*tail_ == nullptr) return;
    T* last = *tail_;
    if (last != nullptr) {
      while (*TLTraits::next(last) != nullptr) {
        last = *TLTraits::next(last);
      }
      tail_ = TLTraits::next(last);
    }
  }

 private:
  T* head_;
  mutable T** tail_;  // We need to ensure a valid `tail_` even when using a
                      // const Iterator.
};

struct EmptyBase {};

// Check ThreadedListBase::EnsureValidTail.
static constexpr bool kUnsafeInsertion = true;

template <typename T, typename TLTraits = ThreadedListTraits<T>>
using ThreadedList = ThreadedListBase<T, EmptyBase, TLTraits>;

template <typename T, typename TLTraits = ThreadedListTraits<T>>
using ThreadedListWithUnsafeInsertions =
    ThreadedListBase<T, EmptyBase, TLTraits, kUnsafeInsertion>;

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_THREADED_LIST_H_
