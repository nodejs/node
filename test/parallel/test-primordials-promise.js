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

Object.defineProperties(Promise.prototype, {
  catch: {
    set: common.mustNotCall('set %Promise.prototype%.catch'),
    get: common.mustNotCall('get %Promise.prototype%.catch'),
  },
  finally: {
    set: common.mustNotCall('set %Promise.prototype%.finally'),
    get: common.mustNotCall('get %Promise.prototype%.finally'),
  },
  then: {
    set: common.mustNotCall('set %Promise.prototype%.then'),
    get: common.mustNotCall('get %Promise.prototype%.then'),
  },
});
Object.defineProperties(Array.prototype, {
  // %Promise.all% and %Promise.allSettled% are depending on the value of
  // `%Array.prototype%.then`.
  then: {},
});
Object.defineProperties(Object.prototype, {
  then: {
    set: common.mustNotCall('set %Object.prototype%.then'),
    get: common.mustNotCall('get %Object.prototype%.then'),
  },
});

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
