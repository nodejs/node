'use strict';
const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

// This test ensures that fast-path PromiseHook assigns async ids
// to already created promises when the native hook function is
// triggered on before event.

let initialAsyncId;
const promise = new Promise((resolve) => {
  setTimeout(() => {
    initialAsyncId = async_hooks.executionAsyncId();
    async_hooks.createHook({
      after: common.mustCall(2)
    }).enable();
    resolve();
  }, 0);
});

promise.then(common.mustCall(() => {
  const id = async_hooks.executionAsyncId();
  assert.notStrictEqual(id, initialAsyncId);
  assert.ok(id > 0);
}));
