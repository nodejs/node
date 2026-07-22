// Flags: --expose-internals
'use strict';

const common = require('../common');
const assert = require('assert');

const {
  PromisePrototypeThen,
  SafePromiseAll,
  SafePromiseAllReturnArrayLike,
  SafePromiseAllReturnVoid,
  SafePromiseAllSettled,
  SafePromiseAllSettledReturnVoid,
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
  then: {
    configurable: true,
    set: common.mustNotCall('set %Array.prototype%.then'),
    get: common.mustNotCall('get %Array.prototype%.then'),
  },
});
Object.defineProperties(Object.prototype, {
  then: {
    set: common.mustNotCall('set %Object.prototype%.then'),
    get: common.mustNotCall('get %Object.prototype%.then'),
  },
});

assertIsPromise(PromisePrototypeThen(test(), common.mustCall()));
assertIsPromise(SafePromisePrototypeFinally(test(), common.mustCall()));

assertIsPromise(SafePromiseAllReturnArrayLike([test()]));
assertIsPromise(SafePromiseAllReturnVoid([test()]));
assertIsPromise(SafePromiseAny([test()]));
assertIsPromise(SafePromiseRace([test()]));

assertIsPromise(SafePromiseAllReturnArrayLike([]));
assertIsPromise(SafePromiseAllReturnVoid([]));

{
  const val1 = Symbol();
  const val2 = Symbol();
  PromisePrototypeThen(
    SafePromiseAllReturnArrayLike([Promise.resolve(val1), { then(resolve) { resolve(val2); } }]),
    common.mustCall((val) => {
      assert.strictEqual(Array.isArray(val), true);
      const expected = [val1, val2];
      assert.deepStrictEqual(val.length, expected.length);
      assert.strictEqual(val[0], expected[0]);
      assert.strictEqual(val[1], expected[1]);
    })
  );
}

{
  // Never settling promises should not block the resulting promise to be rejected:
  const error = new Error();
  PromisePrototypeThen(
    SafePromiseAllReturnArrayLike([new Promise(() => {}), Promise.reject(error)]),
    common.mustNotCall('Should have rejected'),
    common.mustCall((err) => {
      assert.strictEqual(err, error);
    })
  );
  PromisePrototypeThen(
    SafePromiseAllReturnVoid([new Promise(() => {}), Promise.reject(error)]),
    common.mustNotCall('Should have rejected'),
    common.mustCall((err) => {
      assert.strictEqual(err, error);
    })
  );
}

Object.defineProperties(Array.prototype, {
  // %Promise.all% and %Promise.allSettled% are depending on the value of
  // `%Array.prototype%.then`.
  then: {
    __proto__: undefined,
    value: undefined,
  },
});

assertIsPromise(SafePromiseAll([test()]));
assertIsPromise(SafePromiseAllSettled([test()]));
assertIsPromise(SafePromiseAllSettledReturnVoid([test()]));

assertIsPromise(SafePromiseAll([]));
assertIsPromise(SafePromiseAllSettled([]));
assertIsPromise(SafePromiseAllSettledReturnVoid([]));

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
  PromisePrototypeThen(promise, common.mustCall());
}
