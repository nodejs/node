// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const s = "x".repeat(70000);

// Regular function with > 64KB default parameter, sloppy eval, block scope
// reference, and inner closure reference (inner closure resolved before block
// scope due to LIFO inner_scope_ list).
assertEquals(1, eval(`
  (function f(b = "${s}", a = 1) {
    eval("");
    {
      let c = 1;
      a;
    }
    return (() => a)();
  })();
`));

// Arrow function with > 64KB default parameter, sloppy eval, block scope
// reference, and inner closure reference.
assertEquals(1, eval(`
  ((b = "${s}", a = 1) => {
    eval("");
    {
      let c = 1;
      a;
    }
    return (() => a)();
  })();
`));

// Direct reference in the VarBlock body resolved before the inner closure.
assertEquals(1, eval(`
  (function f(b = "${s}", a = 1) {
    eval("");
    a;
    return (() => a)();
  })();
`));

assertEquals(1, eval(`
  ((b = "${s}", a = 1) => {
    eval("");
    a;
    return (() => a)();
  })();
`));

// Non-simple parameters with context-allocated variable in the body.
(function f(b = 0, a = 1) {
  let captured = 2;
  eval("");
  {
    let c = 1;
    assertEquals(1, a);
  }
  assertEquals(3, (() => a + captured)());
})();

((b = 0, a = 1) => {
  let captured = 2;
  eval("");
  {
    let c = 1;
    assertEquals(1, a);
  }
  assertEquals(3, (() => a + captured)());
})();
