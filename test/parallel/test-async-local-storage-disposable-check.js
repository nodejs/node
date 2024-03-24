'use strict';

const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage, AsyncResource } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();
const resource = new AsyncResource('test');

// This test verifies that an error is thrown when an
// `asyncLocalStorage.disposableStore` created in an async resource scope is
// not disposed.

try {
  resource.runInAsyncScope(() => {
    // If the disposable is not modified with `using` statement or being disposed after
    // the current tick is finished, an error is thrown.
    const disposable = asyncLocalStorage.disposableStore(123);

    process.on('uncaughtException', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_ASYNC_LOCAL_STORAGE_NOT_DISPOSED');
      assert.strictEqual(err.disposableStores.size, 1);
      assert.ok(err.disposableStores.has(disposable));
    }));
  });
} catch {
  assert.fail('unreachable');
}
