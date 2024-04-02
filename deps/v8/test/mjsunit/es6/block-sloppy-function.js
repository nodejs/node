// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

(function shadowingLetDoesntBindGenerator() {
  let f = function *f() {
    while(true) {
      yield 1;
    }
  };
  assertEquals(1, f().next().value);
  {
    function *f() {
      while(true) {
        yield 2;
      }
    }
    assertEquals(2, f().next().value);
  }
  assertEquals(1, f().next().value);
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

(function complexParams(a = 0) {
  {
    let y = 3;
    function f(b = 0) {
      y = 2;
    }
    f();
    assertEquals(2, y);
  }
  assertEquals('function', typeof f);
})();

(function complexVarParams(a = 0) {
  var f;
  {
    let y = 3;
    function f(b = 0) {
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

(function executionOrder() {
  function getOuter() {
    return f;
  }
  assertEquals('undefined', typeof getOuter());

  {
    assertEquals('function', typeof f);
    assertEquals('undefined', typeof getOuter());
    function f () {}
    assertEquals('function', typeof f);
    assertEquals('function', typeof getOuter());
  }

  assertEquals('function', typeof getOuter());
})();

(function reassignBindings() {
  function getOuter() {
    return f;
  }
  assertEquals('undefined', typeof getOuter());

  {
    assertEquals('function', typeof f);
    assertEquals('undefined', typeof getOuter());
    f = 1;
    assertEquals('number', typeof f);
    assertEquals('undefined', typeof getOuter());
    function f () {}
    assertEquals('number', typeof f);
    assertEquals('number', typeof getOuter());
    f = '';
    assertEquals('string', typeof f);
    assertEquals('number', typeof getOuter());
  }

  assertEquals('number', typeof getOuter());
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


// Don't shadow simple parameter
(function shadowingParameterDoesntBind(x) {
  assertEquals(1, x);
  {
    function x() {}
  }
  assertEquals(1, x);
})(1);

// Don't shadow complex parameter
(function shadowingDefaultParameterDoesntBind(x = 0) {
  assertEquals(1, x);
  {
    function x() {}
  }
  assertEquals(1, x);
})(1);

// Don't shadow nested complex parameter
(function shadowingNestedParameterDoesntBind([[x]]) {
  assertEquals(1, x);
  {
    function x() {}
  }
  assertEquals(1, x);
})([[1]]);

// Don't shadow rest parameter
(function shadowingRestParameterDoesntBind(...x) {
  assertArrayEquals([1], x);
  {
    function x() {}
  }
  assertArrayEquals([1], x);
})(1);

// Don't shadow complex rest parameter
(function shadowingComplexRestParameterDoesntBind(...[x]) {
  assertArrayEquals(1, x);
  {
    function x() {}
  }
  assertArrayEquals(1, x);
})(1);

// Previous tests with a var declaration thrown in.
// Don't shadow simple parameter
(function shadowingVarParameterDoesntBind(x) {
  var x;
  assertEquals(1, x);
  {
    function x() {}
  }
  assertEquals(1, x);
})(1);

// Don't shadow complex parameter
(function shadowingVarDefaultParameterDoesntBind(x = 0) {
  var x;
  assertEquals(1, x);
  {
    function x() {}
  }
  assertEquals(1, x);
})(1);

// Don't shadow nested complex parameter
(function shadowingVarNestedParameterDoesntBind([[x]]) {
  var x;
  assertEquals(1, x);
  {
    function x() {}
  }
  assertEquals(1, x);
})([[1]]);

// Don't shadow rest parameter
(function shadowingVarRestParameterDoesntBind(...x) {
  var x;
  assertArrayEquals([1], x);
  {
    function x() {}
  }
  assertArrayEquals([1], x);
})(1);

// Don't shadow complex rest parameter
(function shadowingVarComplexRestParameterDoesntBind(...[x]) {
  var x;
  assertArrayEquals(1, x);
  {
    function x() {}
  }
  assertArrayEquals(1, x);
})(1);


// Hoisting is not affected by other simple parameters
(function irrelevantParameterBinds(y, z) {
  assertEquals(undefined, x);
  {
    function x() {}
  }
  assertEquals('function', typeof x);
})(1);

// Hoisting is not affected by other complex parameters
(function irrelevantComplexParameterBinds([y] = [], z) {
  assertEquals(undefined, x);
  {
    function x() {}
  }
  assertEquals('function', typeof x);
})();

// Hoisting is not affected by rest parameters
(function irrelevantRestParameterBinds(y, ...z) {
  assertEquals(undefined, x);
  {
    function x() {}
  }
  assertEquals('function', typeof x);
})();

// Hoisting is not affected by complex rest parameters
(function irrelevantRestParameterBinds(y, ...[z]) {
  assertEquals(undefined, x);
  {
    function x() {}
  }
  assertEquals('function', typeof x);
})();


// Test that shadowing function name is fine
{
  let called = false;
  (function shadowFunctionName() {
    if (called) assertUnreachable();
    called = true;
    {
      function shadowFunctionName() {
        return 0;
      }
      assertEquals(0, shadowFunctionName());
    }
    assertEquals(0, shadowFunctionName());
  })();
}

{
  let called = false;
  (function shadowFunctionNameWithComplexParameter(...r) {
    if (called) assertUnreachable();
    called = true;
    {
      function shadowFunctionNameWithComplexParameter() {
        return 0;
      }
      assertEquals(0, shadowFunctionNameWithComplexParameter());
    }
    assertEquals(0, shadowFunctionNameWithComplexParameter());
  })();
}

(function shadowOuterVariable() {
  {
    let f = 0;
    (function () {
      assertEquals(undefined, f);
      {
        assertEquals(1, f());
        function f() { return 1; }
        assertEquals(1, f());
      }
      assertEquals(1, f());
    })();
    assertEquals(0, f);
  }
})();

(function notInDefaultScope() {
  var y = 1;
  (function innerNotInDefaultScope(x = y) {
    assertEquals('undefined', typeof y);
    {
      function y() {}
    }
    assertEquals('function', typeof y);
    assertEquals(1, x);
  })();
})();

(function noHoistingThroughNestedLexical() {
  {
    let f = 2;
    {
      let y = 3;
      function f() {
        y = 2;
      }
      f();
      assertEquals(2, y);
    }
    assertEquals(2, f);
  }
  assertThrows(()=>f, ReferenceError);
})();

// Only the first function is hoisted; the second is blocked by the first.
// Contrast overridingLocalFunction, in which the outer function declaration
// is not lexical and so the inner declaration is hoisted.
(function noHoistingThroughNestedFunctions() {
  assertEquals(undefined, f); // Also checks that the var-binding exists

  {
    assertEquals(4, f());

    function f() {
      return 4;
    }

    {
      assertEquals(5, f());
      function f() {
        return 5;
      }
      assertEquals(5, f());
    }

    assertEquals(4, f());
  }

  assertEquals(4, f());
})();

// B.3.5 interacts with B.3.3 to allow this.
(function hoistingThroughSimpleCatch() {
  assertEquals(undefined, f);

  try {
    throw 0;
  } catch (f) {
    {
      assertEquals(4, f());

      function f() {
        return 4;
      }

      assertEquals(4, f());
    }

    assertEquals(0, f);
  }

  assertEquals(4, f());
})();

(function noHoistingIfLetOutsideSimpleCatch() {
  assertThrows(()=>f, ReferenceError);

  let f = 2;

  assertEquals(2, f);

  try {
    throw 0;
  } catch (f) {
    {
      assertEquals(4, f());

      function f() {
        return 4;
      }

      assertEquals(4, f());
    }

    assertEquals(0, f);
  }

  assertEquals(2, f);
})();

(function noHoistingThroughComplexCatch() {
  try {
    throw 0;
  } catch ({f}) {
    {
      assertEquals(4, f());

      function f() {
        return 4;
      }

      assertEquals(4, f());
    }
  }

  assertThrows(()=>f, ReferenceError);
})();

(function hoistingThroughWith() {
  with ({f: 0}) {
    assertEquals(0, f);

    {
      assertEquals(4, f());

      function f() {
        return 4;
      }

      assertEquals(4, f());
    }

    assertEquals(0, f);
  }

  assertEquals(4, f());
})();

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

(function evalHoistingThroughSimpleCatch() {
  try {
    throw 0;
  } catch (f) {
    eval(`{ function f() {
      return 4;
    } }`);

    assertEquals(0, f);
  }

  assertEquals(4, f());
})();

(function evalHoistingThroughWith() {
  with ({f: 0}) {
    eval(`{ function f() {
      return 4;
    } }`);

    assertEquals(0, f);
  }

  assertEquals(4, f());
})();

let dontHoistGlobal;
{ function dontHoistGlobal() {} }
assertEquals(undefined, dontHoistGlobal);

let dontHoistEval;
var throws = false;
try {
  eval("{ function dontHoistEval() {} }");
} catch (e) {
  throws = true;
}
assertFalse(throws);

// When the global object is frozen, silently don't hoist
// Currently this actually throws BUG(v8:4452)
Object.freeze(this);
{
  let throws = false;
  try {
    eval('{ function hoistWhenFrozen() {} }');
  } catch (e) {
    throws = true;
  }
  assertFalse(this.hasOwnProperty("hoistWhenFrozen"));
  assertThrows(() => hoistWhenFrozen, ReferenceError);
  // Should be assertFalse BUG(v8:4452)
  assertTrue(throws);
}
