'use strict';

const common = require('../common');

const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

// This test verifies that async local storage works with thenables

const store = new AsyncLocalStorage();
const data = Symbol('verifier');

const then = common.mustCall((cb) => {
  assert.strictEqual(store.getStore(), data);
  setImmediate(cb);
}, 4);

function thenable() {
  return {
    then,
  };
}

// Await a thenable
store.run(data, async () => {
  assert.strictEqual(store.getStore(), data);
  await thenable();
  assert.strictEqual(store.getStore(), data);
});

// Returning a thenable in an async function
store.run(data, async () => {
  try {
    assert.strictEqual(store.getStore(), data);
    return thenable();
  } finally {
    assert.strictEqual(store.getStore(), data);
  }
});

// Resolving a thenable
store.run(data, () => {
  assert.strictEqual(store.getStore(), data);
  Promise.resolve(thenable());
  assert.strictEqual(store.getStore(), data);
});

// Returning a thenable in a then handler
store.run(data, () => {
  assert.strictEqual(store.getStore(), data);
  Promise.resolve().then(() => thenable());
  assert.strictEqual(store.getStore(), data);
});
