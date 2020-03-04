'use strict';
require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();
const outer = {};
const inner = {};

function testInner() {
  assert.strictEqual(asyncLocalStorage.getStore(), outer);

  asyncLocalStorage.run(inner, () => {
    assert.strictEqual(asyncLocalStorage.getStore(), inner);
  });
  assert.strictEqual(asyncLocalStorage.getStore(), outer);

  asyncLocalStorage.exit(() => {
    assert.strictEqual(asyncLocalStorage.getStore(), undefined);
  });
  assert.strictEqual(asyncLocalStorage.getStore(), outer);

  asyncLocalStorage.runSyncAndReturn(inner, () => {
    assert.strictEqual(asyncLocalStorage.getStore(), inner);
  });
  assert.strictEqual(asyncLocalStorage.getStore(), outer);

  asyncLocalStorage.exitSyncAndReturn(() => {
    assert.strictEqual(asyncLocalStorage.getStore(), undefined);
  });
  assert.strictEqual(asyncLocalStorage.getStore(), outer);
}

asyncLocalStorage.run(outer, testInner);
assert.strictEqual(asyncLocalStorage.getStore(), undefined);

asyncLocalStorage.runSyncAndReturn(outer, testInner);
assert.strictEqual(asyncLocalStorage.getStore(), undefined);
