// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation


v = {};
v.__proto__ = new Int32Array(1);
function foo() {
  for (var i = 0; i < 2; i++) {
    v[i] = 0;
  }
}
foo();
assertEquals(Object.keys(v).length, 1);
