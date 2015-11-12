// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-legacy-const --harmony-sloppy --harmony-sloppy-let
// Flags: --harmony-sloppy-function --harmony-destructuring
// Flags: --harmony-rest-parameters

// Test Annex B 3.3 semantics for functions declared in blocks in sloppy mode.
// http://www.ecma-international.org/ecma-262/6.0/#sec-block-level-function-declarations-web-legacy-compatibility-semantics

(function overridingLocalFunction() {
  var x = [];
  assertEquals('function', typeof f);
  function f() {
    x.push(1);
  }
  f();
  {
    f();
    function f() {
      x.push(2);
    }
    f();
  }
  f();
  {
    f();
    function f() {
      x.push(3);
    }
    f();
  }
  f();
  assertArrayEquals([1, 2, 2, 2, 3, 3, 3], x);
})();

(function newFunctionBinding() {
  var x = [];
  assertEquals('undefined', typeof f);
  {
    f();
    function f() {
      x.push(2);
    }
    f();
  }
  f();
  {
    f();
    function f() {
      x.push(3);
    }
    f();
  }
  f();
  assertArrayEquals([2, 2, 2, 3, 3, 3], x);
})();

(function shadowingLetDoesntBind() {
  let f = 1;
  assertEquals(1, f);
  {
    let y = 3;
    function f() {
      y = 2;
    }
    f();
    assertEquals(2, y);
  }
  assertEquals(1, f);
})();

(function shadowingClassDoesntBind() {
  class f { }
  assertEquals('class f { }', f.toString());
  {
    let y = 3;
    function f() {
      y = 2;
    }
    f();
    assertEquals(2, y);
  }
  assertEquals('class f { }', f.toString());
})();

(function shadowingConstDoesntBind() {
  const f = 1;
  assertEquals(1, f);
  {
    let y = 3;
    function f() {
      y = 2;
    }
    f();
    assertEquals(2, y);
  }
  assertEquals(1, f);
})();

(function shadowingVarBinds() {
  var f = 1;
  assertEquals(1, f);
  {
    let y = 3;
    function f() {
      y = 2;
    }
    f();
    assertEquals(2, y);
  }
  assertEquals('function', typeof f);
})();

(function conditional() {
  if (true) {
    function f() { return 1; }
  } else {
    function f() { return 2; }
  }
  assertEquals(1, f());

  if (false) {
    function g() { return 1; }
  } else {
    function g() { return 2; }
  }
  assertEquals(2, g());
})();

(function skipExecution() {
  {
    function f() { return 1; }
  }
  assertEquals(1, f());
  {
    function f() { return 2; }
  }
  assertEquals(2, f());
  L: {
    assertEquals(3, f());
    break L;
    function f() { return 3; }
  }
  assertEquals(2, f());
})();

// Test that hoisting from blocks doesn't happen in global scope
function globalUnhoisted() { return 0; }
{
  function globalUnhoisted() { return 1; }
}
assertEquals(0, globalUnhoisted());

// Test that shadowing arguments is fine
(function shadowArguments(x) {
  assertArrayEquals([1], arguments);
  {
    assertEquals('function', typeof arguments);
    function arguments() {}
    assertEquals('function', typeof arguments);
  }
  assertEquals('function', typeof arguments);
})(1);

// Shadow function parameter
(function shadowParameter(x) {
  assertEquals(1, x);
  {
    function x() {}
  }
  assertEquals('function', typeof x);
})(1);

// Shadow function parameter
(function shadowDefaultParameter(x = 0) {
  assertEquals(1, x);
  {
    function x() {}
  }
  // TODO(littledan): Once destructured parameters are no longer
  // let-bound, enable this assertion. This is the core of the test.
  // assertEquals('function', typeof x);
})(1);

(function shadowRestParameter(...x) {
  assertArrayEquals([1], x);
  {
    function x() {}
  }
  // TODO(littledan): Once destructured parameters are no longer
  // let-bound, enable this assertion. This is the core of the test.
  // assertEquals('function', typeof x);
})(1);

assertThrows(function notInDefaultScope(x = y) {
  {
    function y() {}
  }
  assertEquals('function', typeof y);
  assertEquals(x, undefined);
}, ReferenceError);
