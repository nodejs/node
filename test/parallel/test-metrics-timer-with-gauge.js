'use strict';

const common = require('../common');

const assert = require('assert');
const { subscribe } = require('node:diagnostics_channel');
const { metrics } = require('node:perf_hooks');
const { createGauge, Gauge, Timer, MetricReport } = metrics;

// Create a gauge for timing
const testGauge = createGauge('test.response.time', { base: 'test' });
assert.ok(testGauge instanceof Gauge);

assert.strictEqual(testGauge.type, 'gauge');
assert.strictEqual(testGauge.name, 'test.response.time');
assert.deepStrictEqual(testGauge.meta, { base: 'test' });
assert.strictEqual(testGauge.channelName, 'metrics:gauge:test.response.time');

// Create timers from the gauge
const a = testGauge.createTimer({ timer: 'a', meta: 'extra' });
const b = testGauge.createTimer({ timer: 'b' });

assert.ok(a instanceof Timer);
assert.ok(b instanceof Timer);

const messages = [
  [50, { base: 'test', timer: 'a', meta: 'extra' }],
  [100, { base: 'test', timer: 'b' }],
];

subscribe(testGauge.channelName, common.mustCall((report) => {
  assert.ok(report instanceof MetricReport);
  assert.strictEqual(report.type, 'gauge');
  assert.strictEqual(report.name, 'test.response.time');
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
