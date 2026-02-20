'use strict';

const common = require('../common');

const assert = require('assert');
const { subscribe } = require('node:diagnostics_channel');
const { metrics } = require('node:perf_hooks');
const { createPullGauge, PullGauge, Metric, MetricReport } = metrics;

// Test values to return from the pull function
const values = [ 1, 5, 10, 4, 6 ];
let currentIndex = 0;

const testPullGauge = createPullGauge('test', () => {
  return values[currentIndex];
}, { base: 'test' });

assert.ok(testPullGauge instanceof PullGauge);
assert.ok(testPullGauge instanceof Metric);
assert.strictEqual(testPullGauge.value, 0);

assert.strictEqual(testPullGauge.type, 'pullGauge');
assert.strictEqual(testPullGauge.name, 'test');
assert.deepStrictEqual(testPullGauge.meta, { base: 'test' });
assert.strictEqual(testPullGauge.channelName, 'metrics:pullGauge:test');

// Subscribe to metric reports
let reportCount = 0;
subscribe(testPullGauge.channelName, common.mustCall((report) => {
  assert.ok(report instanceof MetricReport);
  assert.strictEqual(report.type, 'pullGauge');
  assert.strictEqual(report.name, 'test');
  assert.ok(report.time > 0);
  assert.strictEqual(report.value, values[reportCount]);

  if (reportCount < values.length - 1) {
    assert.deepStrictEqual(report.meta, { base: 'test' });
  } else {
    // Last sample includes additional metadata
    assert.deepStrictEqual(report.meta, { base: 'test', extra: 'metadata' });
  }

  reportCount++;
}, values.length));

// Test sampling
for (let i = 0; i < values.length; i++) {
  currentIndex = i;

  if (i === values.length - 1) {
    // Test sampling with additional metadata
    const value = testPullGauge.sample({ extra: 'metadata' });
    assert.strictEqual(value, values[i]);
  } else {
    const value = testPullGauge.sample();
    assert.strictEqual(value, values[i]);
  }
}

// Verify all reports were received
assert.strictEqual(reportCount, values.length);
