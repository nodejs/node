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

class BaseClass {
  constructor(x) {
    this.name_ = x;
  }
  get name() { return this.name_; }
};

class DeferredSuperCall extends BaseClass {
  constructor(x) {
    return async() => super(x);
  }
};

assertEqualsAsync(
  "LexicalSuperCall",
  () => new DeferredSuperCall("LexicalSuperCall")().then(x => x.name));


class DeferredSuperProperty extends BaseClass {
  deferredName() { return async() => super.name; }
};

assertEqualsAsync(
  "LexicalSuperProperty",
  () => new DeferredSuperProperty("LexicalSuperProperty").deferredName()());
