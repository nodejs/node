'use strict';
const common = require('../common');
const assert = require('assert');
const { promiseHooks } = require('v8');

assert.throws(() => {
  promiseHooks.onAfter(async function() { });
}, /The "afterHook" argument must be of type function/);

assert.throws(() => {
  promiseHooks.onAfter(async function*() { });
}, /The "afterHook" argument must be of type function/);

let seen;

const stop = promiseHooks.onAfter(common.mustCall((promise) => {
  seen = promise;
}, 1));

const promise = Promise.resolve().then(() => {
  assert.strictEqual(seen, undefined);
});

promise.then(() => {
  assert.strictEqual(seen, promise);
  stop();
});

assert.strictEqual(seen, undefined);
