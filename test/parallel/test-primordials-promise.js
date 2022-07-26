// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');

const {
  PromisePrototypeThen,
  SafePromiseAll,
  SafePromiseAllSettled,
  SafePromiseAny,
  SafePromisePrototypeFinally,
  SafePromiseRace,
} = require('internal/test/binding').primordials;

Array.prototype[Symbol.iterator] = common.mustNotCall('%Array.prototype%[@@iterator]');
Promise.all = common.mustNotCall('%Promise%.all');
Promise.allSettled = common.mustNotCall('%Promise%.allSettled');
Promise.any = common.mustNotCall('%Promise%.any');
Promise.race = common.mustNotCall('%Promise%.race');
Promise.prototype.catch = common.mustNotCall('%Promise.prototype%.catch');
Promise.prototype.finally = common.mustNotCall('%Promise.prototype%.finally');
Promise.prototype.then = common.mustNotCall('%Promise.prototype%.then');

assertIsPromise(PromisePrototypeThen(test(), common.mustCall()));
assertIsPromise(SafePromisePrototypeFinally(test(), common.mustCall()));

assertIsPromise(SafePromiseAll([test()]));
assertIsPromise(SafePromiseAllSettled([test()]));
assertIsPromise(SafePromiseAny([test()]));
assertIsPromise(SafePromiseRace([test()]));

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
