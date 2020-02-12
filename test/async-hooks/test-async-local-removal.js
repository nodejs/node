'use strict';

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { AsyncLocal } = async_hooks;

// This test ensures correct work of the global hook
// that serves for propagation of all `AsyncLocal`s
// in the context of `.disable()` call

const asyncLocalOne = new AsyncLocal();
asyncLocalOne.store(1);
const asyncLocalTwo = new AsyncLocal();
asyncLocalTwo.store(2);

setImmediate(() => {
  // Removal of one local should not affect others
  asyncLocalTwo.disable();
  assert.strictEqual(asyncLocalOne.unwrap(), 1);
  assert.strictEqual(asyncLocalTwo.unwrap(), undefined);

  // Removal of the last active local should not
  // prevent propagation of locals created later
  asyncLocalOne.disable();
  const asyncLocalThree = new AsyncLocal();
  asyncLocalThree.store(3);
  setImmediate(() => {
    assert.strictEqual(asyncLocalThree.unwrap(), 3);
  });
});
