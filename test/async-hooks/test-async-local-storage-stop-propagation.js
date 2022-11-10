'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage, AsyncResource } = require('async_hooks');

let cnt = 0;
function onPropagate(type, store) {
  assert.strictEqual(als.getStore(), store);
  cnt++;
  if (cnt === 1) {
    assert.strictEqual(type, 'r1');
    return true;
  }
  if (cnt === 2) {
    assert.strictEqual(type, 'r2');
    return false;
  }
}

const als = new AsyncLocalStorage({
  onPropagate: common.mustCall(onPropagate, 2)
});

const myStore = {};

als.run(myStore, common.mustCall(() => {
  const r1 = new AsyncResource('r1');
  const r2 = new AsyncResource('r2');
  r1.runInAsyncScope(common.mustCall(() => {
    assert.strictEqual(als.getStore(), myStore);
  }));
  r2.runInAsyncScope(common.mustCall(() => {
    assert.strictEqual(als.getStore(), undefined);
    r1.runInAsyncScope(common.mustCall(() => {
      assert.strictEqual(als.getStore(), myStore);
    }));
  }));
}));
