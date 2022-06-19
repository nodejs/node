'use strict';
const common = require('../common');
const assert = require('assert');
const { promiseHooks } = require('v8');

assert.throws(() => {
  promiseHooks.onSettled(async function() { });
}, /The "settledHook" argument must be of type function/);

assert.throws(() => {
  promiseHooks.onSettled(async function*() { });
}, /The "settledHook" argument must be of type function/);

let seen;

const stop = promiseHooks.onSettled(common.mustCall((promise) => {
  seen = promise;
}, 4));

// Constructor resolve triggers hook
const promise = new Promise((resolve, reject) => {
  assert.strictEqual(seen, undefined);
  setImmediate(() => {
    resolve();
    assert.strictEqual(seen, promise);
    seen = undefined;

    constructorReject();
  });
});

// Constructor reject triggers hook
function constructorReject() {
  const promise = new Promise((resolve, reject) => {
    assert.strictEqual(seen, undefined);
    setImmediate(() => {
      reject();
      assert.strictEqual(seen, promise);
      seen = undefined;

      simpleResolveReject();
    });
  });
  promise.catch(() => {});
}

// Sync resolve/reject helpers trigger hook
function simpleResolveReject() {
  const resolved = Promise.resolve();
  assert.strictEqual(seen, resolved);
  seen = undefined;

  const rejected = Promise.reject();
  assert.strictEqual(seen, rejected);
  seen = undefined;

  stop();
  rejected.catch(() => {});
}
