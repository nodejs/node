// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testRestIndex() {
  assertEquals(5, (function(...args) { return args.length; })(1,2,3,4,5));
  assertEquals(4, (function(a, ...args) { return args.length; })(1,2,3,4,5));
  assertEquals(3, (function(a, b, ...args) { return args.length; })(1,2,3,4,5));
  assertEquals(2, (function(a, b, c, ...args) {
                      return args.length; })(1,2,3,4,5));
  assertEquals(1, (function(a, b, c, d, ...args) {
                      return args.length; })(1,2,3,4,5));
  assertEquals(0, (function(a, b, c, d, e, ...args) {
                    return args.length; })(1,2,3,4,5));
})();


var strictTest = (function() {
  "use strict";
  return function strictTest(a, b, ...c) {
    assertEquals(Array, c.constructor);
    assertTrue(Array.isArray(c));

    var expectedLength = arguments.length >= 3 ? arguments.length - 2 : 0;
    assertEquals(expectedLength, c.length);

    for (var i = 2, j = 0; i < arguments.length; ++i) {
      assertEquals(c[j++], arguments[i]);
    }
  };
})();


function sloppyTest(a, b, ...c) {
  assertEquals(Array, c.constructor);
  assertTrue(Array.isArray(c));

  var expectedLength = arguments.length >= 3 ? arguments.length - 2 : 0;
  assertEquals(expectedLength, c.length);

  for (var i = 2, j = 0; i < arguments.length; ++i) {
    assertEquals(c[j++], arguments[i]);
  }
}


var O = {
  strict: strictTest,
  sloppy: sloppyTest
};

(function testStrictRestParamArity() {
  assertEquals(2, strictTest.length);
  assertEquals(2, O.strict.length);
})();


(function testRestParamsStrictMode() {
  strictTest();
  strictTest(1, 2);
  strictTest(1, 2, 3, 4, 5, 6);
  strictTest(1, 2, 3);
  O.strict();
  O.strict(1, 2);
  O.strict(1, 2, 3, 4, 5, 6);
  O.strict(1, 2, 3);
})();


(function testRestParamsStrictModeApply() {
  strictTest.apply(null, []);
  strictTest.apply(null, [1, 2]);
  strictTest.apply(null, [1, 2, 3, 4, 5, 6]);
  strictTest.apply(null, [1, 2, 3]);
  O.strict.apply(O, []);
  O.strict.apply(O, [1, 2]);
  O.strict.apply(O, [1, 2, 3, 4, 5, 6]);
  O.strict.apply(O, [1, 2, 3]);
})();


(function testRestParamsStrictModeCall() {
  strictTest.call(null);
  strictTest.call(null, 1, 2);
  strictTest.call(null, 1, 2, 3, 4, 5, 6);
  strictTest.call(null, 1, 2, 3);
  O.strict.call(O);
  O.strict.call(O, 1, 2);
  O.strict.call(O, 1, 2, 3, 4, 5, 6);
  O.strict.call(O, 1, 2, 3);
})();


(function testsloppyRestParamArity() {
  assertEquals(2, sloppyTest.length);
  assertEquals(2, O.sloppy.length);
})();


(function testRestParamssloppyMode() {
  sloppyTest();
  sloppyTest(1, 2);
  sloppyTest(1, 2, 3, 4, 5, 6);
  sloppyTest(1, 2, 3);
  O.sloppy();
  O.sloppy(1, 2);
  O.sloppy(1, 2, 3, 4, 5, 6);
  O.sloppy(1, 2, 3);
})();


(function testRestParamssloppyModeApply() {
  sloppyTest.apply(null, []);
  sloppyTest.apply(null, [1, 2]);
  sloppyTest.apply(null, [1, 2, 3, 4, 5, 6]);
  sloppyTest.apply(null, [1, 2, 3]);
  O.sloppy.apply(O, []);
  O.sloppy.apply(O, [1, 2]);
  O.sloppy.apply(O, [1, 2, 3, 4, 5, 6]);
  O.sloppy.apply(O, [1, 2, 3]);
})();


(function testRestParamssloppyModeCall() {
  sloppyTest.call(null);
  sloppyTest.call(null, 1, 2);
  sloppyTest.call(null, 1, 2, 3, 4, 5, 6);
  sloppyTest.call(null, 1, 2, 3);
  O.sloppy.call(O);
  O.sloppy.call(O, 1, 2);
  O.sloppy.call(O, 1, 2, 3, 4, 5, 6);
  O.sloppy.call(O, 1, 2, 3);
})();


(function testUnmappedArguments() {
  // Strict/Unmapped arguments should always be used for functions with rest
  // parameters
  assertThrows(function(...rest) { return arguments.caller; }, TypeError);
  assertThrows(function(...rest) { return arguments.callee; }, TypeError);
  // TODO(caitp): figure out why this doesn't throw sometimes, even though the
  //              getter always does =)
  // assertThrows(function(...rest) { arguments.caller = 1; }, TypeError);
  // assertThrows(function(...rest) { arguments.callee = 1; }, TypeError);
})();


(function testNoAliasArgumentsStrict() {
  ((function() {
    "use strict";
    return (function strictF(a, ...rest) {
              arguments[0] = 1;
              assertEquals(3, a);
              arguments[1] = 2;
              assertArrayEquals([4, 5], rest);
            });
  })())(3, 4, 5);
})();


(function testNoAliasArgumentsSloppy() {
  function sloppyF(a, ...rest) {
    arguments[0] = 1;
    assertEquals(3, a);
    arguments[1] = 2;
    assertArrayEquals([4, 5], rest);
  }
  sloppyF(3, 4, 5);
})();


(function testRestParamsWithNewTarget() {
  "use strict";
  class Base {
    constructor(...a) {
      this.base = a;
      assertEquals(arguments.length, a.length);
      var args = [];
      for (var i = 0; i < arguments.length; ++i) {
        args.push(arguments[i]);
      }
      assertEquals(args, a);
    }
  }
  class Child extends Base {
    constructor(...b) {
      super(1, 2, 3);
      this.child = b;
      assertEquals(arguments.length, b.length);
      var args = [];
      for (var i = 0; i < arguments.length; ++i) {
        args.push(arguments[i]);
      }
      assertEquals(args, b);
    }
  }

  var c = new Child(1, 2, 3);
  assertEquals([1, 2, 3], c.child);
  assertEquals([1, 2, 3], c.base);
})();

(function TestDirectiveThrows() {
  "use strict";

  assertThrows(
    function(){ eval("function(...rest){'use strict';}") }, SyntaxError);
  assertThrows(function(){ eval("(...rest) => {'use strict';}") }, SyntaxError);
  assertThrows(
    function(){ eval("(class{foo(...rest) {'use strict';}});") }, SyntaxError);

  assertThrows(
    function(){ eval("function(a, ...rest){'use strict';}") }, SyntaxError);
  assertThrows(
    function(){ eval("(a, ...rest) => {'use strict';}") }, SyntaxError);
  assertThrows(
    function(){ eval("(class{foo(a, ...rest) {'use strict';}});") },
    SyntaxError);
})();

(function TestRestArrayPattern() {
  function f(...[a, b, c]) { return a + b + c; }
  assertEquals(6, f(1, 2, 3));
  assertEquals("123", f(1, "2", 3));
  assertEquals(NaN, f(1));

  var f2 = (...[a, b, c]) => a + b + c;
  assertEquals(6, f2(1, 2, 3));
  assertEquals("123", f2(1, "2", 3));
  assertEquals(NaN, f2(1));
})();

(function TestRestObjectPattern() {
  function f(...{length, 0: firstName, 1: lastName}) {
    return `Hello ${lastName}, ${firstName}! Called with ${length} args!`;
  }
  assertEquals("Hello Ross, Bob! Called with 4 args!", f("Bob", "Ross", 0, 0));

  var f2 = (...{length, 0: firstName, 1: lastName}) =>
      `Hello ${lastName}, ${firstName}! Called with ${length} args!`;
  assertEquals("Hello Ross, Bob! Called with 4 args!", f2("Bob", "Ross", 0, 0));
})();
