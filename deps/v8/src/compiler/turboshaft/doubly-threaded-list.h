// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_DOUBLY_THREADED_LIST_H_
#define V8_COMPILER_TURBOSHAFT_DOUBLY_THREADED_LIST_H_

#include "src/base/logging.h"

// DoublyThreadedList is an intrusive doubly-linked list that threads through
// its nodes, somewhat like ThreadedList (src/base/threaded-list.h).
//
// Of interest is the fact that instead of having regular next/prev pointers,
// nodes have a regular "next" pointer, but their "prev" pointer contains the
// address of the "next" of the previous element. This way, removing an element
// doesn't require special treatment for the head of the list, and does not
// even require to know the head of the list.

template <typename T>
struct DoublyThreadedListTraits {
  static T** prev(T t) { return t->prev(); }
  static T* next(T t) { return t->next(); }
  static bool non_empty(T t) { return t != nullptr; }
};

template <class T, class DTLTraits = DoublyThreadedListTraits<T>>
class DoublyThreadedList {
 public:
  // Defining move constructor so that when resizing container, the prev pointer
  // of the next(head_) doesn't point to the old head_ but rather to the new
  // one.
  DoublyThreadedList(DoublyThreadedList&& other) V8_NOEXCEPT {
    head_ = other.head_;
    if (DTLTraits::non_empty(head_)) {
      *DTLTraits::prev(head_) = &head_;
    }
    other.head_ = {};
  }
  DoublyThreadedList() = default;

  // Add `x` at the begining of the list. `x` will not be visible to any
  // existing iterator. Does not invalidate any existing iterator.
  void Add(T x) {
    DCHECK(empty(*DTLTraits::next(x)));
    DCHECK_EQ(*DTLTraits::prev(x), nullptr);
    *DTLTraits::next(x) = head_;
    *DTLTraits::prev(x) = &head_;
    if (DTLTraits::non_empty(head_)) {
      *DTLTraits::prev(head_) = DTLTraits::next(x);
    }
    head_ = x;
  }

  // Removes `x` from the list. Iterators that are currently on `x` are
  // invalidated. To remove while iterating, use RemoveAt.
  static void Remove(T x) {
    if (*DTLTraits::prev(x) == nullptr) {
      DCHECK(empty(*DTLTraits::next(x)));
      // {x} already removed from the list.
      return;
    }
    T** prev = DTLTraits::prev(x);
    T* next = DTLTraits::next(x);
    **prev = *next;
    if (DTLTraits::non_empty(*next)) *DTLTraits::prev(*next) = *prev;
    *DTLTraits::prev(x) = nullptr;
    *DTLTraits::next(x) = {};
  }

  bool empty() const { return !DTLTraits::non_empty(head_); }

  // Since C++17, it is possible to have a sentinel end-iterator that is not an
  // iterator itself.
  class end_iterator {};

  class iterator {
   public:
    explicit iterator(T head) : curr_(head) {}

    T operator*() { return curr_; }

    iterator& operator++() {
      DCHECK(DTLTraits::non_empty(curr_));
      curr_ = *DTLTraits::next(curr_);
      return *this;
    }

    bool operator!=(end_iterator) { return DTLTraits::non_empty(curr_); }

   private:
    friend DoublyThreadedList;
    T curr_;
  };

  iterator begin() const { return iterator{head_}; }
  end_iterator end() const { return end_iterator{}; }

  // Removes the element at `it`, and make `it` point to the next element.
  // Iterators on the same element as `it` are invalidated. Other iterators are
  // not affected.
  iterator RemoveAt(iterator& it) {
    DCHECK(DTLTraits::non_empty(it.curr_));
    T curr = *it;
    T next = *DTLTraits::next(curr);
    Remove(curr);
    return iterator{next};
  }

 private:
  static bool empty(T x) { return !DTLTraits::non_empty(x); }
  T head_{};
};

#endif  // V8_COMPILER_TURBOSHAFT_DOUBLY_THREADED_LIST_H_
