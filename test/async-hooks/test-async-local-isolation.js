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
  assert.strictEqual(asyncLocalOne.unwrap(), undefined);
  assert.strictEqual(asyncLocalTwo.unwrap(), undefined);

  asyncLocalOne.store('foo');
  asyncLocalTwo.store('bar');
  assert.strictEqual(asyncLocalOne.unwrap(), 'foo');
  assert.strictEqual(asyncLocalTwo.unwrap(), 'bar');

  asyncLocalOne.store('baz');
  asyncLocalTwo.store(42);
  setTimeout(() => {
    assert.strictEqual(asyncLocalOne.unwrap(), 'baz');
    assert.strictEqual(asyncLocalTwo.unwrap(), 42);
  }, 0);
}, 0);
