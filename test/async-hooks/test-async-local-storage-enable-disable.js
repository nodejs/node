'use strict';
require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

asyncLocalStorage.runSyncAndReturn(() => {
  asyncLocalStorage.getStore().set('foo', 'bar');
  process.nextTick(() => {
    assert.strictEqual(asyncLocalStorage.getStore().get('foo'), 'bar');
    asyncLocalStorage.disable();
    assert.strictEqual(asyncLocalStorage.getStore(), undefined);
    process.nextTick(() => {
      assert.strictEqual(asyncLocalStorage.getStore(), undefined);
      asyncLocalStorage.runSyncAndReturn(() => {
        assert.notStrictEqual(asyncLocalStorage.getStore(), undefined);
      });
    });
  });
});
