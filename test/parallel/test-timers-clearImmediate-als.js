'use strict';
const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

// This is an asynclocalstorage variant of test-timers-clearImmediate.js
const asyncLocalStorage = new AsyncLocalStorage();
const N = 3;

function next() {
  const fn = common.mustCall(onImmediate);
  asyncLocalStorage.run(new Map(), common.mustCall(() => {
    const immediate = setImmediate(fn);
    const store = asyncLocalStorage.getStore();
    store.set('immediate', immediate);
  }));
}

function onImmediate() {
  const store = asyncLocalStorage.getStore();
  const immediate = store.get('immediate');
  assert.strictEqual(immediate.constructor.name, 'Immediate');
  clearImmediate(immediate);
}

for (let i = 0; i < N; i++) {
  next();
}
