'use strict';

require('../common');
const assert = require('assert');
const { metrics } = require('node:perf_hooks');
const { Timer } = metrics;

// Test that Timer properties are read-only
const timer = new Timer(() => {});

// Verify initial values
assert.ok(typeof timer.start === 'number');
assert.ok(timer.start > 0);
assert.strictEqual(timer.end, undefined);
assert.strictEqual(timer.duration, undefined);

// Try to modify properties (should throw)
const originalStart = timer.start;
assert.throws(() => {
  timer.start = 0;
}, TypeError);
assert.strictEqual(timer.start, originalStart); // Should remain unchanged

assert.throws(() => {
  timer.end = 123;
}, TypeError);
assert.strictEqual(timer.end, undefined); // Should remain undefined

assert.throws(() => {
  timer.duration = 456;
}, TypeError);
assert.strictEqual(timer.duration, undefined); // Should remain undefined

// Stop the timer and verify values are still read-only
timer.stop();
assert.ok(typeof timer.end === 'number');
assert.ok(typeof timer.duration === 'number');
assert.ok(timer.end > timer.start);
assert.ok(timer.duration > 0);

// Try to modify after stopping (should throw)
const stoppedEnd = timer.end;
const stoppedDuration = timer.duration;
assert.throws(() => {
  timer.end = 0;
}, TypeError);
assert.throws(() => {
  timer.duration = 0;
}, TypeError);
assert.strictEqual(timer.end, stoppedEnd);
assert.strictEqual(timer.duration, stoppedDuration);
