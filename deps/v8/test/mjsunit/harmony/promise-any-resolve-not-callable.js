// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-promise-any

load('test/mjsunit/test-async.js');

// Promise.any should call IteratorClose if Promise.resolve is not callable.

let returnCount = 0;
let iter = {
  [Symbol.iterator]() {
    return {
      return() {
        returnCount++;
      }
    };
  }
};

Promise.resolve = "certainly not callable";

testAsync(assert => {
  assert.plan(2);
  Promise.any(iter).then(assert.unreachable, reason => {
    assert.equals(true, reason instanceof TypeError);
    assert.equals(1, returnCount);
  });
});
