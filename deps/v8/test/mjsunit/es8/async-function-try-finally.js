// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function assertThrowsAsync(run, errorType, message) {
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

  if (!hadError) {
    throw new MjsUnitAssertionError(
        "Expected " + run + "() to throw " + errorType.name +
        ", but did not throw.");
  }
  if (!(actual instanceof errorType))
    throw new MjsUnitAssertionError(
        "Expected " + run + "() to throw " + errorType.name +
        ", but threw '" + actual + "'");
  if (message !== void 0 && actual.message !== message)
    throw new MjsUnitAssertionError(
        "Expected " + run + "() to throw '" + message + "', but threw '" +
        actual.message + "'");
};

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

function resolveLater(value) {
  return new Promise(function(resolve) {
    resolve(value);
  });
}

function rejectLater(reason) {
  return new Promise(function(resolve, reject) {
    reject(reason);
  });
}

class MyError extends Error {};
var AsyncFunction = async function() {}.constructor;

assertEqualsAsync("finally-return (func-expr)", async function() {
  try {
    return "early-return (func-expr)";
  } finally {
    return "finally-return (func-expr)";
  }
});

assertEqualsAsync("finally-return (arrow)", async() => {
  try {
    return "early-return (arrow)";
  } finally {
    return "finally-return (arrow)";
  }
});

assertEqualsAsync("finally-return (eval)", AsyncFunction(`
  try {
    return "early-return (eval)";
  } finally {
    return "finally-return (eval)";
  }
`));

assertEqualsAsync("promise-finally-return (func-expr)", async function() {
  try {
    return new Promise(function() {});
  } finally {
    return "promise-finally-return (func-expr)";
  }
});

assertEqualsAsync("promise-finally-return (arrow)", async() => {
  try {
    return new Promise(function() {});
  } finally {
    return "promise-finally-return (arrow)";
  }
});

assertEqualsAsync("promise-finally-return (eval)", AsyncFunction(`
  try {
    return new Promise(function() {});
  } finally {
    return "promise-finally-return (eval)";
  }
`));

assertEqualsAsync("await-finally-return (func-expr)", async function() {
  try {
    return "early-return (func-expr)";
  } finally {
    return await resolveLater("await-finally-return (func-expr)");
  }
});

assertEqualsAsync("await-finally-return (arrow)", async() => {
  try {
    return "early-return (arrow)";
  } finally {
    return await resolveLater("await-finally-return (arrow)");
  }
});

assertEqualsAsync("await-finally-return (eval)", AsyncFunction(`
  try {
    return "early-return (eval)";
  } finally {
    return await resolveLater("await-finally-return (eval)");
  }
`));

assertThrowsAsync(async function() {
  try {
    return "early-return (func-expr)";
  } finally {
    throw new MyError("finally-throw (func-expr)");
  }
}, MyError, "finally-throw (func-expr)");

assertThrowsAsync(async() => {
  try {
    return "early-return (arrow)";
  } finally {
    throw new MyError("finally-throw (arrow)");
  }
}, MyError, "finally-throw (arrow)");

assertThrowsAsync(AsyncFunction(`
  try {
    return "early-return (eval)";
  } finally {
    throw new MyError("finally-throw (eval)");
  }
`), MyError, "finally-throw (eval)");

assertThrowsAsync(async function() {
  try {
    return "early-return (func-expr)";
  } finally {
    await rejectLater(new MyError("await-finally-throw (func-expr)"));
  }
}, MyError, "await-finally-throw (func-expr)");

assertThrowsAsync(async() => {
  try {
    return "early-return (arrow)";
  } finally {
    await rejectLater(new MyError("await-finally-throw (arrow)"));
  }
}, MyError, "await-finally-throw (arrow)");

assertThrowsAsync(AsyncFunction(`
  try {
    return "early-return (eval)";
  } finally {
    await rejectLater(new MyError("await-finally-throw (eval)"));
  }
`), MyError, "await-finally-throw (eval)");
