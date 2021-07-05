'use strict';
const common = require('../common');
const assert = require('assert');
const promiseHooks = require('promise_hooks');

const expected = [];

function testHook(name) {
  const hook = promiseHooks[name];
  const error = new Error(`${name} error`);

  const stop = hook(common.mustCall(() => {
    throw error;
  }));

  expected.push([ error, stop ]);
}

process.on('uncaughtException', common.mustCall((received) => {
  const [error, stop] = expected.shift();
  assert.strictEqual(received, error);
  stop();
}, 4));

testHook('onInit');
testHook('onResolve');
testHook('onBefore');
testHook('onAfter');

Promise.resolve().then(() => {});
