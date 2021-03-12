// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_FUNCTIONAL_LIST_H_
#define V8_COMPILER_FUNCTIONAL_LIST_H_

#include <iterator>
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

// A generic stack implemented as a purely functional singly-linked list, which
// results in an O(1) copy operation. It is the equivalent of functional lists
// in ML-like languages, with the only difference that it also caches the length
// of the list in each node.
// TODO(turbofan): Use this implementation also for RedundancyElimination.
template <class A>
class FunctionalList {
 private:
  struct Cons : ZoneObject {
    Cons(A top, Cons* rest)
        : top(std::move(top)), rest(rest), size(1 + (rest ? rest->size : 0)) {}
    A const top;
    Cons* const rest;
    size_t const size;
  };

 public:
  FunctionalList() : elements_(nullptr) {}

  bool operator==(const FunctionalList<A>& other) const {
    if (Size() != other.Size()) return false;
    iterator it = begin();
    iterator other_it = other.begin();
    while (true) {
      if (it == other_it) return true;
      if (*it != *other_it) return false;
      ++it;
      ++other_it;
    }
  }
  bool operator!=(const FunctionalList<A>& other) const {
    return !(*this == other);
  }

  bool TriviallyEquals(const FunctionalList<A>& other) const {
    return elements_ == other.elements_;
  }

  const A& Front() const {
    DCHECK_GT(Size(), 0);
    return elements_->top;
  }

  FunctionalList Rest() const {
    FunctionalList result = *this;
    result.DropFront();
    return result;
  }

  void DropFront() {
    CHECK_GT(Size(), 0);
    elements_ = elements_->rest;
  }

  void PushFront(A a, Zone* zone) {
    elements_ = zone->New<Cons>(std::move(a), elements_);
  }

  // If {hint} happens to be exactly what we want to allocate, avoid allocation
  // by reusing {hint}.
  void PushFront(A a, Zone* zone, FunctionalList hint) {
    if (hint.Size() == Size() + 1 && hint.Front() == a &&
        hint.Rest() == *this) {
      *this = hint;
    } else {
      PushFront(a, zone);
    }
  }

  // Drop elements until the current stack is equal to the tail shared with
  // {other}. The shared tail must not only be equal, but also refer to the
  // same memory.
  void ResetToCommonAncestor(FunctionalList other) {
    while (other.Size() > Size()) other.DropFront();
    while (other.Size() < Size()) DropFront();
    while (elements_ != other.elements_) {
      DropFront();
      other.DropFront();
    }
  }

  size_t Size() const { return elements_ ? elements_->size : 0; }

  void Clear() { elements_ = nullptr; }

  class iterator : public std::iterator<std::forward_iterator_tag, A> {
   public:
    explicit iterator(Cons* cur) : current_(cur) {}

    const A& operator*() const { return current_->top; }
    iterator& operator++() {
      current_ = current_->rest;
      return *this;
    }
    bool operator==(const iterator& other) const {
      return this->current_ == other.current_;
    }
    bool operator!=(const iterator& other) const { return !(*this == other); }

    // Implemented so that std::find and friends can use std::iterator_traits
    // for this iterator type.
    typedef std::forward_iterator_tag iterator_category;
    typedef ptrdiff_t difference_type;
    typedef A value_type;
    typedef A* pointer;
    typedef A& reference;

   private:
    Cons* current_;
  };

  iterator begin() const { return iterator(elements_); }
  iterator end() const { return iterator(nullptr); }

 private:
  Cons* elements_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_FUNCTIONAL_LIST_H_
