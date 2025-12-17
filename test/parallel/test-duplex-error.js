'use strict';

const common = require('../common');
const assert = require('assert');
const { duplexPair } = require('stream');

const [sideA, sideB] = duplexPair();

sideA.on('error', common.mustCall((err) => {
  assert.strictEqual(err.message, 'Simulated error');
}));

sideB.on('error', common.mustCall((err) => {
  assert.strictEqual(err.message, 'Simulated error');
}));

sideA.resume();
sideB.resume();

sideB.destroy(new Error('Simulated error'));
