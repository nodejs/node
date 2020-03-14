'use strict';
require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

async function foo() {}

const asyncLocalStorage = new AsyncLocalStorage();

async function testOut() {
  await foo();
  assert.strictEqual(asyncLocalStorage.getStore(), undefined);
}

async function testAwait() {
  await foo();
  assert.notStrictEqual(asyncLocalStorage.getStore(), undefined);
  assert.strictEqual(asyncLocalStorage.getStore().get('key'), 'value');
  await asyncLocalStorage.exit(testOut);
}

asyncLocalStorage.run(new Map(), () => {
  const store = asyncLocalStorage.getStore();
  store.set('key', 'value');
  testAwait(); // should not reject
});
assert.strictEqual(asyncLocalStorage.getStore(), undefined);
