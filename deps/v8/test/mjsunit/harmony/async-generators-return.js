// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-async-iteration --allow-natives-syntax

testAsync(test => {
  test.plan(2);

  async function* gen() {
    return;
    test.unreachable();
  }

  let didResolvePromise = false;
  gen().next().then(
      (iterResult) => {
        test.equals(false, didResolvePromise);
        test.equals({ value: undefined, done: true }, iterResult);
      },
      test.unexpectedRejection());

  // Race: generator request's promise should resolve before this Promise.
  Promise.resolve("already-resolved").then(
      _ => { didResolvePromise = true },
      test.unexpectedRejection());

}, "return-race-no-operand");

testAsync(test => {
  test.plan(2);

  async function* gen() {
    return undefined;
    test.unreachable();
  }

  let didResolvePromise = false;
  gen().next().then(
      (iterResult) => {
        test.equals(true, didResolvePromise);
        test.equals({ value: undefined, done: true }, iterResult);
      },
      test.unexpectedRejection());

  // Race: generator request's promise should resolve after this Promise.
  Promise.resolve("already-resolved").then(
      _ => { didResolvePromise = true },
      test.unexpectedRejection());

}, "return-race-with-operand");

// Return a thenable which is never settled
testAsync(test => {
  test.plan(0);

  let promise = { then() { } };

  async function* gen() {
    return promise;
    test.unreachable();
  }

  gen().next().then(
      (iterResult) => test.unreachable(),
      test.unexpectedRejection());
}, "return-await-thenable-pending");

// Return a thenable which is fulfilled later
testAsync(test => {
  test.plan(2);

  let resolve;
  let awaitedThenable = { then(resolveFn) { resolve = resolveFn; } };
  let finallyEvaluated = false;

  async function* gen() {
    try {
      return awaitedThenable;
    } finally {
      finallyEvaluated = true;
    }
  }

  gen().next().then(
      (iterResult) => {
        test.equals({ value: "resolvedPromise", done: true }, iterResult);
        test.equals(true, finallyEvaluated);
      },
      test.unexpectedRejection());

  test.drainMicrotasks();
  resolve("resolvedPromise");
}, "yield-await-thenable-resolved");

// Return a thenable which is rejected later
testAsync(test => {
  test.plan(2);

  let reject;
  let awaitedThenable = { then(resolveFn, rejectFn) { reject = rejectFn; } };
  async function* gen() {
    try {
      yield awaitedThenable;
    } catch (e) {
      test.equals("rejection", e);
      return e;
    }
  }

  gen().next().then(
      (iterResult) => {
        test.equals({ value: "rejection", done: true }, iterResult);
      },
      test.unexpectedRejection());

  test.drainMicrotasks();
  reject("rejection");
}, "yield-await-thenable-rejected");
