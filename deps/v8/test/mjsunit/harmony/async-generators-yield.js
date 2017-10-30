// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-async-iteration --allow-natives-syntax

// Yield a thenable which is never settled
testAsync(test => {
  test.plan(0);

  let awaitedThenable = { then() { } };

  async function* gen() {
    yield awaitedThenable;
    test.unreachable();
  }

  gen().next().then(
      (iterResult) => test.unreachable(),
      test.unexpectedRejection());
}, "yield-await-thenable-pending");

// Yield a thenable which is fulfilled later
testAsync(test => {
  test.plan(1);

  let resolve;
  let awaitedThenable = { then(resolveFn) { resolve = resolveFn; } };

  async function* gen() {
    let input = yield awaitedThenable;
    test.equals("resolvedPromise", input);
  }

  gen().next().then(
      (iterResult) => {
        test.equals({ value: "resolvedPromise", done: false }, iterResult);
      },
      test.unexpectedRejection());

  test.drainMicrotasks();
  resolve("resolvedPromise");
}, "yield-await-thenable-resolved");

// Yield a thenable which is rejected later
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
