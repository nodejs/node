// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

(function TestMigrateUninitializedDoubleFieldToTagged() {
  let o1 = {c: 1, a: 1.5};
  function f(x) {
    return {c: 1, a: x};
  }
  f(2.5);
  f(2.5);

  let o2 = {c: 1.5, a: "str"};
  let res = f(3.5);
  assertEquals(1, res.c);
  assertEquals(3.5, res.a);
})();
