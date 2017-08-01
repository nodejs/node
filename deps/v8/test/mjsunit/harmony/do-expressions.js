// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-do-expressions --allow-natives-syntax --no-always-opt
// Flags: --opt

function returnValue(v) { return v; }
function MyError() {}
var global = this;

function TestBasic() {
  // Looping and lexical declarations
  assertEquals(512, returnValue(do {
    let n = 2;
    for (let i = 0; i < 4; i++) n <<= 2;
  }));

  // Strings do the right thing
  assertEquals("spooky halloween", returnValue(do {
    "happy halloween".replace('happy', 'spooky');
  }));

  // Do expressions with no completion produce an undefined value
  assertEquals(undefined, returnValue(do {}));
  assertEquals(undefined, returnValue(do { var x = 99; }));
  assertEquals(undefined, returnValue(do { function f() {}; }));
  assertEquals(undefined, returnValue(do { let z = 33; }));

  // Propagation of exception
  assertThrows(function() {
    (do {
      throw new MyError();
      "potatoes";
    });
  }, MyError);

  assertThrows(function() {
    return do {
      throw new MyError();
      "potatoes";
    };
  }, MyError);

  // Return value within do-block overrides `return |do-expression|`
  assertEquals("inner-return", (function() {
    return "outer-return" + do {
      return "inner-return";
      "";
    };
  })());

  var count = 0, n = 1;
  // Breaking out |do-expression|
  assertEquals(3, (function() {
    for (var i = 0; i < 10; ++i) (count += 2 * do { if (i === 3) break; ++n });
    return i;
  })());
  // (2 * 2) + (2 * 3) + (2 * 4)
  assertEquals(18, count);

  // Continue in |do-expression|
  count = 0, n = 1;
  assertEquals([1, 3, 5, 7, 9], (function() {
    var values = [];
    for (var i = 0; i < 10; ++i) {
      count += 2 * (do {
        if ((i & 1) === 0) continue;
        values.push(i);
        ++n;
      }) + 1;
    }
    // (2*2) + 1 + (2*3) + 1 + (2*4) + 1 + (2*5) + 1 + (2*6) + 1
    return values;
  })());
  assertEquals(count, 45);

  assertThrows("(do { break; });", SyntaxError);
  assertThrows("(do { continue; });", SyntaxError);

  // Real-world use case for desugaring
  var array = [1, 2, 3, 4, 5], iterable = [6, 7, 8,9];
  assertEquals([1, 2, 3, 4, 5, 6, 7, 8, 9], do {
    for (var element of iterable) array.push(element);
    array;
  });

  // Nested do-expressions
  assertEquals(125, do { (do { (do { 5 * 5 * 5 }) }) });

  // Directives are not honoured
  (do {
    "use strict";
    foo = 80;
    assertEquals(foo, 80);
  });

  // Non-empty operand stack testing
  var O = {
    method1() {
      let x = 256;
      return x + do {
        for (var i = 0; i < 4; ++i) x += i;
      } + 17;
    },
    method2() {
      let x = 256;
      this.reset();
      return x + do {
        for (var i = 0; i < this.length(); ++i) x += this.index() * 2;
      };
    },
    _index: 0,
    index() {
      return ++this._index;
    },
    _length: 4,
    length() { return this._length; },
    reset() { this._index = 0; }
  };
  assertEquals(535, O["method" + do { 1 } + ""]());
  assertEquals(532, O["method" + do { ({ valueOf() { return "2"; } }); }]());
  assertEquals(532, O[
      do { let s = ""; for (let c of "method") s += c; } + "2"]());
}
TestBasic();


function TestDeoptimization1() {
  function f(v) {
    return 88 + do {
      v.a * v.b + v.c;
    };
  }

  var o1 = {};
  o1.a = 10;
  o1.b = 5;
  o1.c = 50;

  var o2 = {};
  o2.c = 100;
  o2.a = 10;
  o2.b = 10;

  assertEquals(188, f(o1));
  assertEquals(188, f(o1));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(188, f(o1));
  assertOptimized(f);
  assertEquals(288, f(o2));
  assertUnoptimized(f);
  assertEquals(288, f(o2));
}
TestDeoptimization1();


function TestInParameterInitializers() {
  var first_name = "George";
  var last_name = "Jetson";
  function fn1(name = do { first_name + " " + last_name }) {
    return name;
  }
  assertEquals("George Jetson", fn1());

  var _items = [1, 2, 3, NaN, 4, 5];
  function fn2(items = do {
    let items = [];
    for (var el of _items) {
      if (el !== el) {
        items;
        break;
      }
      items.push(el), items;
    }
  }) {
    return items;
  }
  assertEquals([1, 2, 3], fn2());

  function thrower() { throw new MyError(); }
  function fn3(exception = do {  try { thrower(); } catch (e) { e } }) {
    return exception;
  }
  assertDoesNotThrow(fn3);
  assertInstanceof(fn3(), MyError);

  function fn4(exception = do { throw new MyError() }) {}
  function catcher(fn) {
    try {
      fn();
      assertUnreachable("fn() initializer should throw");
    } catch (e) {
      assertInstanceof(e, MyError);
    }
  }
  catcher(fn4);
}
TestInParameterInitializers();


function TestWithEval() {
  (function sloppy1() {
    assertEquals(do { eval("var x = 5"), x }, 5);
    assertEquals(x, 5);
  })();

  assertThrows(function strict1() {
    "use strict";
    (do { eval("var x = 5"), x }, 5);
  }, ReferenceError);

  assertThrows(function strict2() {
    (do { eval("'use strict'; var x = 5"), x }, 5);
  }, ReferenceError);
}
TestWithEval();


function TestHoisting() {
  (do { var a = 1; });
  assertEquals(a, 1);
  assertEquals(global.a, undefined);

  (do {
    for (let it of [1, 2, 3, 4, 5]) {
      var b = it;
    }
  });
  assertEquals(b, 5);
  assertEquals(global.b, undefined);

  {
    let x = 1

    // TODO(caitp): ensure VariableStatements in |do-expressions| in parameter
    // initializers, are evaluated in the same VariableEnvironment as they would
    // be for eval().
    // function f1(a = do { var x = 2 }, b = x) { return b }
    // assertEquals(1, f1())

    // function f2(a = x, b = do { var x = 2 }) { return a }
    // assertEquals(1, f2())

    function f3({a = do { var x = 2 }, b = x}) { return b }
    assertEquals(2, f3({}))

    function f4({a = x, b = do { var x = 2 }}) { return b }
    assertEquals(undefined, f4({}))

    function f5(a = do { var y = 0 }) {}
    assertThrows(() => y, ReferenceError)
  }

  // TODO(caitp): Always block-scope function declarations in |do| expressions
  //(do {
  //  assertEquals(true, inner_func());
  //  function inner_func() { return true; }
  //});
  //assertThrows(function() { return innerFunc(); }, ReferenceError);
}
TestHoisting();


// v8:4661

function tryFinallySimple() { (do { try {} finally {} }); }
tryFinallySimple();
tryFinallySimple();
tryFinallySimple();
tryFinallySimple();

var finallyRanCount = 0;
function tryFinallyDoExpr() {
  return (do {
    try {
      throw "BOO";
    } catch (e) {
      "Caught: " + e + " (" + finallyRanCount + ")"
    } finally {
      ++finallyRanCount;
    }
  });
}
assertEquals("Caught: BOO (0)", tryFinallyDoExpr());
assertEquals(1, finallyRanCount);
assertEquals("Caught: BOO (1)", tryFinallyDoExpr());
assertEquals(2, finallyRanCount);
assertEquals("Caught: BOO (2)", tryFinallyDoExpr());
assertEquals(3, finallyRanCount);
assertEquals("Caught: BOO (3)", tryFinallyDoExpr());
assertEquals(4, finallyRanCount);


function TestOSR() {
  var numbers = do {
    let nums = [];
    for (let i = 0; i < 1000; ++i) {
      let value = (Math.random() * 100) | 0;
      nums.push(value === 0 ? 1 : value), nums;
    }
  };
  assertEquals(numbers.length, 1000);
}

for (var i = 0; i < 64; ++i) TestOSR();
