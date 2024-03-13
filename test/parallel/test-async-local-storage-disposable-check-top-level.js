'use strict';

const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

// This test verifies that an error is thrown when an
// `asyncLocalStorage.disposableStore` created in the top level async scope is
// not disposed.

{
  // If the disposable is not modified with `using` statement or being disposed after
  // the current tick is finished, an error is thrown.
  const disposable = asyncLocalStorage.disposableStore(123);
  process.on('uncaughtException', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_ASYNC_LOCAL_STORAGE_NOT_DISPOSED');
    assert.strictEqual(err.disposableStores.size, 1);
    assert.ok(err.disposableStores.has(disposable));
  }));
}
