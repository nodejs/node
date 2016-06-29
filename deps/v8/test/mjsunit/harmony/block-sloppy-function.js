// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-sloppy --harmony-sloppy-let
// Flags: --harmony-sloppy-function

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

// Test that hoisting from blocks does happen in global scope
function globalHoisted() { return 0; }
{
  function globalHoisted() { return 1; }
}
assertEquals(1, globalHoisted());

// Also happens when not previously defined
assertEquals(undefined, globalUndefinedHoisted);
{
  function globalUndefinedHoisted() { return 1; }
}
assertEquals(1, globalUndefinedHoisted());
var globalUndefinedHoistedDescriptor =
    Object.getOwnPropertyDescriptor(this, "globalUndefinedHoisted");
assertFalse(globalUndefinedHoistedDescriptor.configurable);
assertTrue(globalUndefinedHoistedDescriptor.writable);
assertTrue(globalUndefinedHoistedDescriptor.enumerable);
assertEquals(1, globalUndefinedHoistedDescriptor.value());

// When a function property is hoisted, it should be
// made enumerable.
// BUG(v8:4451)
Object.defineProperty(this, "globalNonEnumerable", {
  value: false,
  configurable: true,
  writable: true,
  enumerable: false
});
eval("{function globalNonEnumerable() { return 1; }}");
var globalNonEnumerableDescriptor
    = Object.getOwnPropertyDescriptor(this, "globalNonEnumerable");
// BUG(v8:4451): Should be made non-configurable
assertTrue(globalNonEnumerableDescriptor.configurable);
assertTrue(globalNonEnumerableDescriptor.writable);
// BUG(v8:4451): Should be made enumerable
assertFalse(globalNonEnumerableDescriptor.enumerable);
assertEquals(1, globalNonEnumerableDescriptor.value());

// When a function property is hoisted, it should be overwritten and
// made writable and overwritten, even if the property was non-writable.
Object.defineProperty(this, "globalNonWritable", {
  value: false,
  configurable: true,
  writable: false,
  enumerable: true
});
eval("{function globalNonWritable() { return 1; }}");
var globalNonWritableDescriptor
    = Object.getOwnPropertyDescriptor(this, "globalNonWritable");
// BUG(v8:4451): Should be made non-configurable
assertTrue(globalNonWritableDescriptor.configurable);
// BUG(v8:4451): Should be made writable
assertFalse(globalNonWritableDescriptor.writable);
assertFalse(globalNonEnumerableDescriptor.enumerable);
// BUG(v8:4451): Should be overwritten
assertEquals(false, globalNonWritableDescriptor.value);

// Test that hoisting from blocks does happen in an eval
eval(`
  function evalHoisted() { return 0; }
  {
    function evalHoisted() { return 1; }
  }
  assertEquals(1, evalHoisted());
`);

// Test that hoisting from blocks happens from eval in a function
!function() {
  eval(`
    function evalInFunctionHoisted() { return 0; }
    {
      function evalInFunctionHoisted() { return 1; }
    }
    assertEquals(1, evalInFunctionHoisted());
  `);
}();

let dontHoistGlobal;
{ function dontHoistGlobal() {} }
assertEquals(undefined, dontHoistGlobal);

let dontHoistEval;
// BUG(v8:) This shouldn't hoist and shouldn't throw
var throws = false;
try {
  eval("{ function dontHoistEval() {} }");
} catch (e) {
  throws = true;
}
assertTrue(throws);

// When the global object is frozen, silently don't hoist
// Currently this actually throws BUG(v8:4452)
Object.freeze(this);
throws = false;
try {
  eval('{ function hoistWhenFrozen() {} }');
} catch (e) {
  throws = true;
}
assertFalse(this.hasOwnProperty("hoistWhenFrozen"));
assertThrows(() => hoistWhenFrozen, ReferenceError);
// Should be assertFalse BUG(v8:4452)
assertTrue(throws);
