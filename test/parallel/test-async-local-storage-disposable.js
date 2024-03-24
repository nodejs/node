'use strict';

require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const asyncLocalStorage = new AsyncLocalStorage();

{
  // This is equivalent to `using asyncLocalStorage.disposableStore(123)`.
  const disposable = asyncLocalStorage.disposableStore(123);
  assert.strictEqual(asyncLocalStorage.getStore(), 123);
  disposable[Symbol.dispose]();
}
assert.strictEqual(asyncLocalStorage.getStore(), undefined);

{
  // [Symbol.dispose] should be a no-op once it has been called.
  const disposable = asyncLocalStorage.disposableStore(123);
  assert.strictEqual(asyncLocalStorage.getStore(), 123);
  disposable[Symbol.dispose]();

  const disposable2 = asyncLocalStorage.disposableStore(456);
  // Call [Symbol.dispose] again
  disposable[Symbol.dispose]();
  assert.strictEqual(asyncLocalStorage.getStore(), 456);

  disposable2[Symbol.dispose]();
}
assert.strictEqual(asyncLocalStorage.getStore(), undefined);
