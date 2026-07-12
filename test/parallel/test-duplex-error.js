'use strict';

const common = require('../common');
const assert = require('assert');
const { duplexPair } = require('stream');

const [sideA, sideB] = duplexPair();

// Side A should receive the error because we called .destroy(err) on it.
sideA.on('error', common.mustCall((err) => {
  assert.strictEqual(err.message, 'Simulated error');
}));

// Side B should NOT necessarily emit an error (to avoid crashing
// existing code), but it MUST be destroyed.
sideB.on('error', common.mustNotCall('Side B should not emit an error event'));

sideB.on('close', common.mustCall(() => {
  assert.strictEqual(sideB.destroyed, true);
}));

sideA.resume();
sideB.resume();

// Trigger the destruction
sideA.destroy(new Error('Simulated error'));

// Check the state in the next tick to allow nextTick/microtasks to run
setImmediate(common.mustCall(() => {
  assert.strictEqual(sideA.destroyed, true);
  assert.strictEqual(sideB.destroyed, true);
}));
