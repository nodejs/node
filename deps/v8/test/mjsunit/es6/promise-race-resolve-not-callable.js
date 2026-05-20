// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/test-async.js');

// Promise.race should not call GetIterator if Promise.resolve is not callable.

let getIteratorCount = 0;
let iter = {
  get [Symbol.iterator]() {
    ++getIteratorCount;
  }
};

Promise.resolve = "certainly not callable";

testAsync(assert => {
  assert.plan(2);
  Promise.race(iter).then(assert.unreachable, reason => {
    assert.equals(true, reason instanceof TypeError);
    assert.equals(0, getIteratorCount);
  });
});
