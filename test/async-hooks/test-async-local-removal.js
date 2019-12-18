'use strict';

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { AsyncLocal } = async_hooks;

// This test ensures correct work of the global hook
// that serves for propagation of all `AsyncLocal`s
// in the context of `.remove()` call

const asyncLocalOne = new AsyncLocal();
asyncLocalOne.set(1);
const asyncLocalTwo = new AsyncLocal();
asyncLocalTwo.set(2);

setImmediate(() => {
  // Removal of one local should not affect others
  asyncLocalTwo.remove();
  assert.strictEqual(asyncLocalOne.get(), 1);

  // Removal of the last active local should not
  // prevent propagation of locals created later
  asyncLocalOne.remove();
  const asyncLocalThree = new AsyncLocal();
  asyncLocalThree.set(3);
  setImmediate(() => {
    assert.strictEqual(asyncLocalThree.get(), 3);
  });
});
