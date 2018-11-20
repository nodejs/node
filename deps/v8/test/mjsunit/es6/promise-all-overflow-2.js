// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Test that pre-allocation of the result array works even if it needs to be
// allocated in large object space.
const a = new Array(64 * 1024);
a.fill(Promise.resolve(1));
testAsync(assert => {
  assert.plan(1);
  Promise.all(a).then(b => {
    assert.equals(a.length, b.length);
  });
});
