// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-promise-any

load('test/mjsunit/test-async.js');

// Test that pre-allocation of the errors array works even if it needs to be
// allocated in large object space.
const a = new Array(64 * 1024);
a.fill(Promise.reject(1));
testAsync(assert => {
  assert.plan(1);
  Promise.any(a).then(assert.unreachable, b => {
    assert.equals(a.length, b.errors.length);
  });
});
