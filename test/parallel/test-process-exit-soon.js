'use strict';

const common = require('../common');
const assert = require('assert');

// Schedule something that'll keep the loop active normally
const timer1 = setInterval(() => {}, 1000);
const timer2 = setInterval(() => {}, 1000);

// Register an exitingSoon handler to clean up before exit
process.on('exitingSoon', common.mustCall((ready, timed) => {
  // timed is false because there is no timer
  assert.strictEqual(timed, false);
  // Simulate some async task
  assert.strictEqual(process.exitCode, 0);
  // Shouldn't be callable twice
  assert.strictEqual(process.exitSoon(0), false);
  setImmediate(() => {
    // Clean up resources
    clearInterval(timer1);
    // Notify that we're done
    ready();
  });
}));

process.once('exitingSoon', common.mustCall((ready) => {
  clearInterval(timer2);
  ready();
}));

assert.strictEqual(process.exitSoon(0), true);
