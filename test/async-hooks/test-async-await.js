'use strict';
const common = require('../common');

// This test ensures async hooks are being properly called
// when using async-await mechanics. This involves:
// 1. Checking that all initialized promises are being resolved
// 2. Checking that for each 'before' corresponding hook 'after' hook is called

const assert = require('assert');
const asyncHooks = require('async_hooks');
const util = require('util');

const sleep = util.promisify(setTimeout);
const promiseCallbacks = new Map();
const resolvedPromises = new Set();

const asyncHook = asyncHooks.createHook({
  init, before, after, promiseResolve
});
asyncHook.enable();

function init(asyncId, type, triggerAsyncId, resource) {
  if (type === 'PROMISE') {
    promiseCallbacks.set(asyncId, 0);
  }
}

function before(asyncId) {
  if (promiseCallbacks.has(asyncId)) {
    assert.strictEqual(promiseCallbacks.get(asyncId), 0,
                       'before hook called for promise without prior call' +
                       'to init hook');
    promiseCallbacks.set(asyncId, 1);
  }
}

function after(asyncId) {
  if (promiseCallbacks.has(asyncId)) {
    assert.strictEqual(promiseCallbacks.get(asyncId), 1,
                       'after hook called for promise without prior call' +
                       'to before hook');
    promiseCallbacks.set(asyncId, 0);
  }
}

function promiseResolve(asyncId) {
  assert(promiseCallbacks.has(asyncId),
         'resolve hook called for promise without prior call to init hook');

  resolvedPromises.add(asyncId);
}

const timeout = common.platformTimeout(10);

function checkPromiseCallbacks() {
  for (const balance of promiseCallbacks.values()) {
    assert.strictEqual(balance, 0,
                       'mismatch between before and after hook calls');
  }
}

function checkPromiseResolution() {
  for (const id of promiseCallbacks.keys()) {
    assert(resolvedPromises.has(id),
           'promise initialized without being resolved');
  }
}

process.on('beforeExit', common.mustCall(() => {
  asyncHook.disable();

  checkPromiseResolution();
  checkPromiseCallbacks();
}));

async function asyncFunc(callback) {
  await sleep(timeout);
}

asyncFunc();
