// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --async-stack-traces

// Basic test with an explicit throw.
(function() {
  const realm = Realm.createAllowCrossRealmAccess();

  async function one(x) {
    await two(x);
  }

  const two = Realm.eval(realm, `(async function two(x) {
    await x;
    throw new Error();
  })`);

  async function test(f) {
    try {
      await f(new Promise(resolve => setTimeout(resolve)));
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, Realm.global(realm).Error);
      assertMatches(/Error.+at two.+at async one.+at async test/ms, e.stack);
    }
  }

  assertPromiseResult((async () => {
    %PrepareFunctionForOptimization(one);
    %PrepareFunctionForOptimization(two);
    await test(one);
    await test(one);
    %OptimizeFunctionOnNextCall(two);
    await test(one);
    %OptimizeFunctionOnNextCall(one);
    await test(one);
    Realm.dispose(realm);
  })());
})();

// Basic test with an implicit throw (via ToNumber on Symbol).
(function() {
  const realm = Realm.createAllowCrossRealmAccess();

  async function one(x) {
    return await two(x);
  }

  const two = Realm.eval(realm, `(async function two(x) {
    await x;
    return +Symbol();  // This will raise a TypeError.
  })`);

  async function test(f) {
    try {
      await f(new Promise(resolve => setTimeout(resolve)));
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, Realm.global(realm).TypeError);
      assertMatches(/TypeError.+at two.+at async one.+at async test/ms, e.stack);
    }
  }

  assertPromiseResult((async() => {
    %PrepareFunctionForOptimization(one);
    %PrepareFunctionForOptimization(two);
    await test(one);
    await test(one);
    %OptimizeFunctionOnNextCall(two);
    await test(one);
    %OptimizeFunctionOnNextCall(one);
    await test(one);
    Realm.dispose(realm);
  })());
})();

// Basic test with async functions and promises chained via
// Promise.prototype.then(), which should still work following
// the generic chain upwards.
(function() {
  const realm = Realm.createAllowCrossRealmAccess();

  async function one(x) {
    return await two(x).then(x => x);
  }

  const two = Realm.eval(realm, `(async function two(x) {
    await x.then(x => x);
    throw new Error();
  })`);

  async function test(f) {
    try {
      await f(new Promise(resolve => setTimeout(resolve)));
      assertUnreachable();
    } catch (e) {
      assertInstanceof(e, Realm.global(realm).Error);
      assertMatches(/Error.+at two.+at async one.+at async test/ms, e.stack);
    }
  }

  assertPromiseResult((async() => {
    %PrepareFunctionForOptimization(one);
    %PrepareFunctionForOptimization(two);
    await test(one);
    await test(one);
    %OptimizeFunctionOnNextCall(two);
    await test(one);
    %OptimizeFunctionOnNextCall(one);
    await test(one);
    Realm.dispose(realm);
  })());
})();
