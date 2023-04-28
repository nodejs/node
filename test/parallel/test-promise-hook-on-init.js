'use strict';
const common = require('../common');
const assert = require('assert');
const { promiseHooks } = require('v8');

assert.throws(() => {
  promiseHooks.onInit(async function() { });
}, /The "initHook" argument must be of type function/);

assert.throws(() => {
  promiseHooks.onInit(async function*() { });
}, /The "initHook" argument must be of type function/);

let seenPromise;
let seenParent;

const stop = promiseHooks.onInit(common.mustCall((promise, parent) => {
  seenPromise = promise;
  seenParent = parent;
}, 2));

const parent = Promise.resolve();
assert.strictEqual(seenPromise, parent);
assert.strictEqual(seenParent, undefined);

const child = parent.then();
assert.strictEqual(seenPromise, child);
assert.strictEqual(seenParent, parent);

seenPromise = undefined;
seenParent = undefined;

stop();

Promise.resolve();
assert.strictEqual(seenPromise, undefined);
assert.strictEqual(seenParent, undefined);
