// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

// Create two distinct iterator types which both have a getter for `next`.
class Iterator1 {
  get next() {
    return () => ({ done: true });
  }
}
class Iterator2 {
  get next() {
    return () => ({ done: true });
  }
}

// Create two iterables which return instances of these two distinct iterators.
const iterable1 = {
  [Symbol.iterator]() {
    return new Iterator1();
  },
};
const iterable2 = {
  [Symbol.iterator]() {
    return new Iterator2();
  },
};

// Iterate the iterable using for-of.
function foo(iterable) {
  for (const x of iterable) {
    return x;
  }
}

// Make foo polymorphic in the iterator, specifically so that the feedback for
// the iterator.next named load is polymorphic, with the feedback being two
// distinct getters.
%PrepareFunctionForOptimization(foo);
foo(iterable1);
foo(iterable2);

// The optimization should be successful and not trigger any DCHECKs, despite
// the iterator.next load being before the for-of's implicit try block, and the
// iterator.next() call being inside it.
%OptimizeMaglevOnNextCall(foo);
foo(iterable1);
