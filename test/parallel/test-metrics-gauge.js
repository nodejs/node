'use strict';

const common = require('../common');

const assert = require('assert');
const { subscribe } = require('node:diagnostics_channel');
const { gauge, Gauge, Metric, MetricReport } = require('node:metrics');

const testGauge = gauge('test', { base: 'test' });
assert.ok(testGauge instanceof Gauge);
assert.ok(testGauge instanceof Metric);
assert.strictEqual(testGauge.value, 0);

assert.strictEqual(testGauge.type, 'gauge');
assert.strictEqual(testGauge.name, 'test');
assert.deepStrictEqual(testGauge.meta, { base: 'test' });
assert.strictEqual(testGauge.channelName, 'metrics:gauge:test');

const messages = [
  [123, { base: 'test', meta: 'first' }],
  [0, { base: 'test' }],
];

subscribe(testGauge.channelName, common.mustCall((report) => {
  assert.ok(report instanceof MetricReport);
  assert.strictEqual(report.type, 'gauge');
  assert.strictEqual(report.name, 'test');
  assert.ok(report.time > 0);

  const [value, meta] = messages.shift();
  assert.strictEqual(report.value, value);
  assert.deepStrictEqual(report.meta, meta);
}, 2));

testGauge.reset(123, { meta: 'first' });
testGauge.reset();
