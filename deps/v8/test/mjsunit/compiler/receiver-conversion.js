// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This test suite checks that the receiver value (i.e. the 'this' binding) is
// correctly converted even when the callee function is inlined. This behavior
// is specified by ES6, section 9.2.1.2 "OrdinaryCallBindThis".

var global = this;
function test(outer, inner, check) {
  %PrepareFunctionForOptimization(outer);
  check(outer());
  check(outer());
  %OptimizeFunctionOnNextCall(outer);
  check(outer());
}


// -----------------------------------------------------------------------------
// Test undefined in sloppy mode.
(function UndefinedSloppy() {
  function check(x) {
    assertEquals("object", typeof x);
    assertSame(global, x);
  }
  function inner(x) {
    return this;
  }
  function outer() {
    return sloppy();
  }
  global.sloppy = inner;
  test(outer, inner, check);
})();


// -----------------------------------------------------------------------------
// Test undefined in strict mode.
(function UndefinedStrict() {
  function check(x) {
    assertEquals("undefined", typeof x);
    assertSame(undefined, x);
  }
  function inner(x) {
    "use strict";
    return this;
  }
  function outer() {
    return strict();
  }
  global.strict = inner;
  test(outer, inner, check);
})();


// -----------------------------------------------------------------------------
// Test primitive number in sloppy mode.
(function NumberSloppy() {
  function check(x) {
    assertEquals("object", typeof x);
    assertInstanceof(x, Number);
  }
  function inner(x) {
    return this;
  }
  function outer() {
    return (0).sloppy();
  }
  Number.prototype.sloppy = inner;
  test(outer, inner, check);
})();


// -----------------------------------------------------------------------------
// Test primitive number in strict mode.
(function NumberStrict() {
  function check(x) {
    assertEquals("number", typeof x);
    assertSame(0, x);
  }
  function inner(x) {
    "use strict";
    return this;
  }
  function outer() {
    return (0).strict();
  }
  Number.prototype.strict = inner;
  test(outer, inner, check);
})();


// -----------------------------------------------------------------------------
// Test primitive string in sloppy mode.
(function StringSloppy() {
  function check(x) {
    assertEquals("object", typeof x);
    assertInstanceof(x, String);
  }
  function inner(x) {
    return this;
  }
  function outer() {
    return ("s").sloppy();
  }
  String.prototype.sloppy = inner;
  test(outer, inner, check);
})();


// -----------------------------------------------------------------------------
// Test primitive string in strict mode.
(function StringStrict() {
  function check(x) {
    assertEquals("string", typeof x);
    assertSame("s", x);
  }
  function inner(x) {
    "use strict";
    return this;
  }
  function outer() {
    return ("s").strict();
  }
  String.prototype.strict = inner;
  test(outer, inner, check);
})();
