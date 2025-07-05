'use strict';

const common = require('../common');

const assert = require('assert');
const { subscribe } = require('node:diagnostics_channel');
const { metrics } = require('node:perf_hooks');
const { createCounter, Counter, Timer, MetricReport } = metrics;

// Create a counter for timing
const testCounter = createCounter('test.duration', { base: 'test' });
assert.ok(testCounter instanceof Counter);

assert.strictEqual(testCounter.type, 'counter');
assert.strictEqual(testCounter.name, 'test.duration');
assert.deepStrictEqual(testCounter.meta, { base: 'test' });
assert.strictEqual(testCounter.channelName, 'metrics:counter:test.duration');

// Create timers from the counter
const a = testCounter.createTimer({ timer: 'a', meta: 'extra' });
const b = testCounter.createTimer({ timer: 'b' });

assert.ok(a instanceof Timer);
assert.ok(b instanceof Timer);

const messages = [
  [50, { base: 'test', timer: 'a', meta: 'extra' }],
  [100, { base: 'test', timer: 'b' }],
];

subscribe(testCounter.channelName, common.mustCall((report) => {
  assert.ok(report instanceof MetricReport);
  assert.strictEqual(report.type, 'counter');
  assert.strictEqual(report.name, 'test.duration');
  assert.ok(report.time > 0);

  const [value, meta] = messages.shift();
  assert.ok(near(report.value, value));
  assert.deepStrictEqual(report.meta, meta);
}, 2));

// NOTE: If this test is flaky, tune the threshold to give more leeway to the timing
function near(actual, expected, threshold = 10) {
  return Math.abs(actual - expected) <= threshold;
}

setTimeout(() => {
  a.stop();
}, 50);

setTimeout(() => {
  b[Symbol.dispose]();
}, 100);
