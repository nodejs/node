// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var o1 = {};
var o2 = {};
var a = [0, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,0,0];
var b = [,,,,,2,3,4];
var c = [o1, o2];
var d = [,,, o2, o1];
var e = [0.5,3,4];
var f = [,,,,0.5,3,4];

function checkIncludes(ary, value) {
  return ary.includes(value)
}

function checkIndexOf(ary, value, expected) {
  return ary.indexOf(value) == expected;
}

function expectIncludes(ary, value) {
  assertTrue(checkIncludes(ary, value));
}

function expectNotIncludes(ary, value) {
  assertFalse(checkIncludes(ary, value));
}

function expectIndexOf(ary, value, expected) {
  assertTrue(checkIndexOf(ary, value, expected));
}

var testIncludes = {
  polymorphic: function() {
    expectIncludes(a, 21);
    expectIncludes(b, 4);
    expectIncludes(c, o2);
    expectIncludes(d, o1);
    expectNotIncludes(a, o1);
    expectNotIncludes(b, o2);
    expectNotIncludes(c, 3);
    expectNotIncludes(d, 4);
    },

  polymorphicDouble: function() {
    expectIncludes(e, 3);
    expectIncludes(f, 0.5);
    expectNotIncludes(e, 10);
    expectNotIncludes(f, 0.25);
  },

  polymorphicMixed: function() {
    expectIncludes(a, 21);
    expectIncludes(b, 4);
    expectIncludes(c, o2);
    expectIncludes(d, o1);
    expectIncludes(e, 3);
    expectIncludes(f, 0.5);
    expectNotIncludes(a, o1);
    expectNotIncludes(b, o2);
    expectNotIncludes(c, 3);
    expectNotIncludes(d, 4);
    expectNotIncludes(e, 10);
    expectNotIncludes(f, 0.25);
  },
};

var testIndexOf = {
  polymorphic: function() {
    expectIndexOf(a, 21, 21);
    expectIndexOf(b, 4, 7);
    expectIndexOf(c, o2, 1);
    expectIndexOf(d, o1, 4);
    expectIndexOf(a, o1, -1);
    expectIndexOf(b, o2, -1);
    expectIndexOf(c, 3, -1);
    expectIndexOf(d, 4, -1);
  },

  polymorphicDouble: function() {
    expectIndexOf(e, 3, 1);
    expectIndexOf(f, 0.5, 4);
    expectIndexOf(e, 10, -1);
    expectIndexOf(f, 0.25, -1);
  },

  polymorphicMixed: function() {
    expectIndexOf(a, 21, 21);
    expectIndexOf(b, 4, 7);
    expectIndexOf(c, o2, 1);
    expectIndexOf(d, o1, 4);
    expectIndexOf(e, 3, 1);
    expectIndexOf(f, 0.5, 4);
    expectIndexOf(a, o1, -1);
    expectIndexOf(b, o2, -1);
    expectIndexOf(c, 3, -1);
    expectIndexOf(d, 4, -1);
    expectIndexOf(e, 10, -1);
    expectIndexOf(f, 0.25, -1);
  },
};

function runTests(tests, func) {
  for (test in tests) {
    %DeoptimizeFunction(func);
    %ClearFunctionFeedback(func);
    %PrepareFunctionForOptimization(func);
    tests[test]();
    %OptimizeFunctionOnNextCall(func);
    tests[test]();
  }
}

runTests(testIncludes, checkIncludes)
runTests(testIndexOf, checkIndexOf)
