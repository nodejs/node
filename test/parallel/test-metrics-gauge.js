'use strict';

const common = require('../common');

const assert = require('assert');
const { subscribe } = require('node:diagnostics_channel');
const { gauge, Gauge, Metric, MetricReport } = require('node:metrics');

const testGauge = gauge('test', { base: 'test' });
assert.ok(testGauge instanceof Gauge);
assert.strictEqual(testGauge.value, 0);

const { metric } = testGauge;
assert.ok(metric instanceof Metric);
assert.strictEqual(metric.type, 'gauge');
assert.strictEqual(metric.name, 'test');
assert.deepStrictEqual(metric.meta, { base: 'test' });
assert.strictEqual(metric.channelName, 'metrics:gauge:test');

const messages = [
  [123, { base: 'test', meta: 'first' }],
  [357, { base: 'test', meta: 'second' }],
  [0, { base: 'test' }],
];

subscribe(metric.channelName, common.mustCall((report) => {
  assert.ok(report instanceof MetricReport);
  assert.strictEqual(report.type, 'gauge');
  assert.strictEqual(report.name, 'test');
  assert.ok(report.time > 0);

  const [value, meta] = messages.shift();
  assert.strictEqual(report.value, value);
  assert.deepStrictEqual(report.meta, meta);
}, 3));

testGauge.reset(123, { meta: 'first' });
testGauge.applyDelta(234, { meta: 'second' });
testGauge.reset();
