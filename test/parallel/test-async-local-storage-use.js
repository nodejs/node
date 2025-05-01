'use strict';

require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

const storage = new AsyncLocalStorage();

const store1 = {};
const store2 = {};
const store3 = {};

function inner() {
  // TODO(bengl): Once `using` is supported use that here and don't call
  // dispose manually later.
  const disposable = storage.use(store2);
  assert.strictEqual(storage.getStore(), store2);
  storage.enterWith(store3);
  disposable[Symbol.dispose]();
}

storage.enterWith(store1);
assert.strictEqual(storage.getStore(), store1);
inner();
assert.strictEqual(storage.getStore(), store1);
