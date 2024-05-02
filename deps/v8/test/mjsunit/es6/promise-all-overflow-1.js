// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

d8.file.execute('test/mjsunit/test-async.js');

// Make sure we properly throw a RangeError when overflowing the maximum
// number of elements for Promise.all, which is capped at 2^21 bits right
// now, since we store the indices as identity hash on the resolve element
// closures.
const a = new Array(2 ** 21 - 1);
const p = Promise.resolve(1);
for (let i = 0; i < a.length; ++i) a[i] = p;
testAsync(assert => {
  assert.plan(1);
  Promise.all(a).then(assert.unreachable, reason => {
    assert.equals(true, reason instanceof RangeError);
  });
});
