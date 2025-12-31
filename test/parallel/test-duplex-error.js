'use strict';

const common = require('../common');
const assert = require('assert');
const { duplexPair } = require('stream');

const [sideA, sideB] = duplexPair();

// Use common.mustCall inside the listeners to ensure they trigger
sideA.on('error', common.mustCall((err) => {
  sideAErrorReceived = true;
}));

sideB.on('error', common.mustCall((err) => {
  sideBErrorReceived = true;
}));

sideA.resume();
sideB.resume();

sideB.destroy(new Error('Simulated error'));

// Wrap the callback in common.mustCall()
setImmediate(common.mustCall(() => {
  assert.strictEqual(sideAErrorReceived, true);
  assert.strictEqual(sideBErrorReceived, true);
}));
