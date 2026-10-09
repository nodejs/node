// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation

(function TestFirstFieldUninitializedDouble() {
  function f(v) {
    return {a: v()};
  }

  f(() => 1.5);
  f(() => 2.5);

  let inner;
  const outer = f(() => {
    inner = f(() => 2.25);
    return 7.75;
  });

  assertEquals(2.25, inner.a);
  assertEquals(7.75, outer.a);

  outer.a = 99.5;
  assertEquals(2.25, inner.a);
  assertEquals(99.5, outer.a);
})();

(function TestSubsequentFieldUninitializedDouble() {
  function f(v) {
    return {x: 1.5, a: v()};
  }

  f(() => 1.5);
  f(() => 2.5);

  let inner;
  const outer = f(() => {
    inner = f(() => 2.25);
    return 7.75;
  });

  assertEquals(2.25, inner.a);
  assertEquals(7.75, outer.a);

  outer.a = 99.5;
  assertEquals(2.25, inner.a);
  assertEquals(99.5, outer.a);
})();
