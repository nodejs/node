// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-rest-parameters

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

function strictTest(a, b, ...c) {
  "use strict";
  assertEquals(Array, c.constructor);
  assertTrue(Array.isArray(c));

  var expectedLength = arguments.length >= 3 ? arguments.length - 2 : 0;
  assertEquals(expectedLength, c.length);

  for (var i = 2, j = 0; i < arguments.length; ++i) {
    assertEquals(c[j++], arguments[i]);
  }
}

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
  function strictF(a, ...rest) {
    "use strict";
    arguments[0] = 1;
    assertEquals(3, a);
    arguments[1] = 2;
    assertArrayEquals([4, 5], rest);
  }
  strictF(3, 4, 5);
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


/* TODO(caitp): support arrow functions (blocked on spread operator support)
(function testRestParamsArrowFunctions() {
  "use strict";

  var fn = (a, b, ...c) => c;
  assertEquals([], fn());
  assertEquals([], fn(1, 2));
  assertEquals([3], fn(1, 2, 3));
  assertEquals([3, 4], fn(1, 2, 3, 4));
  assertEquals([3, 4, 5], fn(1, 2, 3, 4, 5));
  assertThrows("var x = ...y => y;", SyntaxError);
  assertEquals([], ((...args) => args)());
  assertEquals([1,2,3], ((...args) => args)(1,2,3));
})();*/
