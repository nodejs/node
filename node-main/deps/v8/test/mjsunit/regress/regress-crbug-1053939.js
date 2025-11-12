// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation


function foo(a, b) {
  a[b] = 1;
  return a[b];
}
v = [];
assertEquals(foo(v, 1), 1);
v.__proto__.__proto__ = new Int32Array();
assertEquals(foo(Object(), 1), 1);
assertEquals(foo(v, 2), undefined);
