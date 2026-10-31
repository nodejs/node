// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const iterator = {
  [Symbol.iterator]() {
    return this;
  },
  next() {
    return { value: 42, done: false };
  },
  return() {
    d8.terminateNow();
    return {};
  }
};

try {
  %IterableForEach(iterator, () => {
    throw new Error("abrupt completion");
  });
} catch (e) {
  // If the termination exception is swallowed, we catch Error("abrupt
  // completion") here.
}

assertUnreachable("Termination exception was swallowed in IteratorClose");
