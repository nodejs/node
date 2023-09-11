'use strict';
const common = require('../common');
const assert = require('assert');
const { promiseHooks } = require('v8');

assert.throws(() => {
  promiseHooks.onBefore(async function() { });
}, /The "beforeHook" argument must be of type function/);

assert.throws(() => {
  promiseHooks.onBefore(async function*() { });
}, /The "beforeHook" argument must be of type function/);

let seen;

const stop = promiseHooks.onBefore(common.mustCall((promise) => {
  seen = promise;
}, 1));

const promise = Promise.resolve().then(() => {
  assert.strictEqual(seen, promise);
  stop();
});

promise.then();

assert.strictEqual(seen, undefined);
