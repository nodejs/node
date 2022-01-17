// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');

const {
  PromisePrototypeCatch,
  PromisePrototypeThen,
  SafePromisePrototypeFinally,
} = require('internal/test/binding').primordials;

Promise.prototype.catch = common.mustNotCall();
Promise.prototype.finally = common.mustNotCall();
Promise.prototype.then = common.mustNotCall();

assertIsPromise(PromisePrototypeCatch(Promise.reject(), common.mustCall()));
assertIsPromise(PromisePrototypeThen(test(), common.mustCall()));
assertIsPromise(SafePromisePrototypeFinally(test(), common.mustCall()));

async function test() {
  const catchFn = common.mustCall();
  const finallyFn = common.mustCall();

  try {
    await Promise.reject();
  } catch {
    catchFn();
  } finally {
    finallyFn();
  }
}

function assertIsPromise(promise) {
  // Make sure the returned promise is a genuine %Promise% object and not a
  // subclass instance.
  assert.strictEqual(Object.getPrototypeOf(promise), Promise.prototype);
}
