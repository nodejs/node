'use strict';

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { AsyncLocal } = async_hooks;

// This test ensures correct work of the global hook
// that serves for propagation of all `AsyncLocal`s
// in the context of `.get()`/`.set(value)` calls

const asyncLocal = new AsyncLocal();

setTimeout(() => {
  assert.strictEqual(asyncLocal.get(), undefined);

  asyncLocal.set('A');
  setTimeout(() => {
    assert.strictEqual(asyncLocal.get(), 'A');

    asyncLocal.set('B');
    setTimeout(() => {
      assert.strictEqual(asyncLocal.get(), 'B');
    }, 0);

    assert.strictEqual(asyncLocal.get(), 'B');
  }, 0);

  assert.strictEqual(asyncLocal.get(), 'A');
}, 0);
