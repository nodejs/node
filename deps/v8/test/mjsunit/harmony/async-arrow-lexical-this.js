// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-async-await --allow-natives-syntax

function assertEqualsAsync(expected, run, msg) {
  var actual;
  var hadValue = false;
  var hadError = false;
  var promise = run();

  if (typeof promise !== "object" || typeof promise.then !== "function") {
    throw new MjsUnitAssertionError(
        "Expected " + run.toString() +
        " to return a Promise, but it returned " + PrettyPrint(promise));
  }

  promise.then(function(value) { hadValue = true; actual = value; },
               function(error) { hadError = true; actual = error; });

  assertFalse(hadValue || hadError);

  %RunMicrotasks();

  if (hadError) throw actual;

  assertTrue(
      hadValue, "Expected '" + run.toString() + "' to produce a value");

  assertEquals(expected, actual, msg);
};

var O = {
  [Symbol.toStringTag]: "LexicalThis",
  run(n) {
    return async passFail => `${n}. ${passFail}: ${this}`;
  },
};

assertEqualsAsync("1. PASS: [object LexicalThis]", () => O.run(1)("PASS"));

var O2 = {
  [Symbol.toStringTag]: "LexicalThis",
  run: O.run(2)
};

assertEqualsAsync("2. PASS: [object LexicalThis]", () => O2.run("PASS"));
