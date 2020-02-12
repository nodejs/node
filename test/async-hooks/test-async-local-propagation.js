'use strict';

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { AsyncLocal } = async_hooks;

// This test ensures correct work of the global hook
// that serves for propagation of all `AsyncLocal`s
// in the context of `.unwrap()`/`.store(value)` calls

const asyncLocal = new AsyncLocal();

setTimeout(() => {
  assert.strictEqual(asyncLocal.unwrap(), undefined);

  asyncLocal.store('A');
  setTimeout(() => {
    assert.strictEqual(asyncLocal.unwrap(), 'A');

    asyncLocal.store('B');
    setTimeout(() => {
      assert.strictEqual(asyncLocal.unwrap(), 'B');
    }, 0);

    assert.strictEqual(asyncLocal.unwrap(), 'B');
  }, 0);

  assert.strictEqual(asyncLocal.unwrap(), 'A');
}, 0);
