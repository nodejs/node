'use strict';
const common = require('../common');
const assert = require('assert');
const promiseHooks = require('promise_hooks');

const expected = [];

function testHook(name) {
  const hook = promiseHooks[name];
  const error = new Error(`${name} error`);

  const stop = hook(common.mustCall(() => {
    stop();
    throw error;
  }));

  expected.push(error);
}

process.on('uncaughtException', common.mustCall((received) => {
  assert.strictEqual(received, expected.shift());
}, 4));

testHook('onResolve');
testHook('onInit');
testHook('onBefore');
testHook('onAfter');

const stop = promiseHooks.onInit(common.mustCall(() => {}, 2));

Promise.resolve().then(stop);
