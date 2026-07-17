// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

import * as mod from "keyed-has-ic-module-export.mjs";

function testIn(obj, key) {
  return key in obj;
}

function expectTrue(obj, key) {
  assertTrue(testIn(obj, key));
}

function expectFalse(obj, key) {
  assertFalse(testIn(obj, key));
}

var tests = {
  monomporphicTrue: function() {
    expectTrue(mod, "a");
    expectTrue(mod, "a");
    expectTrue(mod, "a");
  },

  polymprohicKeyTrue: function() {
    expectTrue(mod, "a");
    expectTrue(mod, "b");
    expectTrue(mod, "c");
  },

  monomorphicFalse: function() {
    expectFalse(mod, "d");
    expectFalse(mod, "d");
    expectFalse(mod, "d");
  },

  polymorphicKeyFalse: function() {
    expectFalse(mod, "d");
    expectFalse(mod, "e");
    expectFalse(mod, "f");
  },

  polymorphicTrue: function() {
    var o = {a: "A"};
    expectTrue(mod, "a");
    expectTrue(o, "a");
    expectTrue(mod, "a");
    expectTrue(o, "a");
  },

  polymorphicFalse: function() {
    var o = {a: "A"};
    expectFalse(mod, "d");
    expectFalse(o, "d");
    expectFalse(mod, "d");
    expectFalse(o, "d");
  }
};

for (let test in tests) {
  %DeoptimizeFunction(testIn);
  %ClearFunctionFeedback(testIn);
  %PrepareFunctionForOptimization(testIn);
  tests[test]();
  %OptimizeFunctionOnNextCall(testIn);
  tests[test]();
}
