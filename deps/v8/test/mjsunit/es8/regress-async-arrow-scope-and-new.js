// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that parenthesized async arrow functions parse cleanly in both lazy and
// eager closures when an outer `let async` binding is present.
function makeLazyClosure() {
  let async = {};
  let fn = () => {
    let a = async () => 42;
    let b = (async (x) => x);
    return [a, b];
  };
  return { fn };
}

let lazy = makeLazyClosure();
assertEquals(2, lazy.fn().length);

let eager = (() => {
  let async = {};
  let fn = (() => {
    let a = async () => 42;
    let b = (async (x) => x);
    return () => [a, b];
  })();
  return { fn };
})();
assertEquals(2, eager.fn().length);

// Test Scope::Snapshot and Reparent when an arrow parameter list calls sloppy
// eval inside a block scope or alongside an outer sloppy eval.
(function TestArrowParamEvalInBlockScope() {
  let outer_var = 10;
  function f() {
    {
      let a = (x = eval("var y = 20")) => [x, y];
      assertEquals([undefined, 20], a());
    }
    let b = (z = (1)) => outer_var + z;
    assertEquals(11, b());
  }
  f();

  function g() {
    let async = 10;
    return (function() {
      {
        let a = async (x = eval("var async = 20")) => async;
      }
      return async;
    })();
  }
  assertEquals(10, g());

  function h() {
    eval("var v = 42");
    {
      let a = (y = 1) => y;
      assertEquals(1, a());
    }
    return v;
  }
  assertEquals(42, h());
})();

// Test that invalid async arrow heads with `new` or parentheses throw
// SyntaxError.
assertThrows("new async()() => {}", SyntaxError);
assertThrows("(async)() => {}", SyntaxError);
