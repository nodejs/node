// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

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

// Rename a function so that it can help omit things from stack trace.
function test(fn) {
  return Object.defineProperty(fn, "name", {
      enumerable: false,
      configurable: true,
      value: "@" + fn.name,
      writable: false
    });
}

function getStack(error) {
  var stack = error.stack.split('\n').
    filter(function(line) {
        return /^\s*at @?[a-zA-Z0-9_]/.test(line);
    }).
    map(line => line.replace(/^\s*at (@?[a-zA-Z0-9_\.\[\]]+)(.*)/, "$1"));

  // remove `Promise.then()` invocation by assertEqualsAsync()
  if (stack[2] === "assertEqualsAsync") return [];

  return stack.reverse();
}

var log = [];
class FakePromise extends Promise {
  constructor(executor) {
    var stack = getStack(new Error("Getting Callstack"));
    if (stack.length) {
      var first = -1;
      for (var i = 0; i < stack.length; ++i) {
        if (stack[i][0] === '@') {
          first = i;
          break;
        }
      }
      while (first > 0) stack.shift(), --first;
      if (stack.length) {
        log.push("@@Species: [" + stack.join(" > ") + "]");
      }
    }
    return new Promise(executor);
  }
};

Object.defineProperty(Promise, Symbol.species, {
  value: FakePromise,
  configurable: true,
  enumerable: false,
  writable: false
});

// Internal `AsyncFunctionAwait` only --- no @@species invocations.
async function asyncFn() { return await "foo"; }
assertEqualsAsync("foo", test(function testInternalOnly() { return asyncFn(); },
                  "should not call Promise[@@Species]"));
assertEquals([], log);

log.length = 0;
assertEqualsAsync(
    "foo",
    test(function testThenOnReturnedPromise() {
      return asyncFn().then(x => (log.push("Then: " + x), x));
    }),
    "should call Promise[@@Species] after non-internal Then");
assertEquals([
  "@@Species: [@testThenOnReturnedPromise > Promise.then > FakePromise]",
  "Then: foo"
], log);
