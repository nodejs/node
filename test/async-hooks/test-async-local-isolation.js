'use strict';

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { AsyncLocal } = async_hooks;

// This test ensures isolation of `AsyncLocal`s
// from each other in terms of stored values

const asyncLocalOne = new AsyncLocal();
const asyncLocalTwo = new AsyncLocal();

setTimeout(() => {
  assert.strictEqual(asyncLocalOne.get(), undefined);
  assert.strictEqual(asyncLocalTwo.get(), undefined);

  asyncLocalOne.set('foo');
  asyncLocalTwo.set('bar');
  assert.strictEqual(asyncLocalOne.get(), 'foo');
  assert.strictEqual(asyncLocalTwo.get(), 'bar');

  asyncLocalOne.set('baz');
  asyncLocalTwo.set(42);
  setTimeout(() => {
    assert.strictEqual(asyncLocalOne.get(), 'baz');
    assert.strictEqual(asyncLocalTwo.get(), 42);
  }, 0);
}, 0);
