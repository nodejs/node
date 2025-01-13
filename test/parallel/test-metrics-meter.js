'use strict';

const common = require('../common');

const assert = require('assert');
const { subscribe } = require('node:diagnostics_channel');
const { meter, Meter, Metric, MetricReport } = require('node:metrics');

const testMeter = meter('test', 100, { base: 'test' });
assert.ok(testMeter instanceof Meter);
assert.strictEqual(testMeter.value, 0);
assert.strictEqual(testMeter.interval, 100);
assert.ok(testMeter.metric instanceof Metric);

const { metric } = testMeter;
assert.strictEqual(metric.type, 'meter');
assert.strictEqual(metric.name, 'test');
assert.deepStrictEqual(metric.meta, { base: 'test' });
assert.strictEqual(metric.channelName, 'metrics:meter:test');

const messages = [
  [1, { base: 'test' }],
  [124, { base: 'test', meta: 'extra' }],
  [1, { base: 'test' }],
];

subscribe(metric.channelName, common.mustCall((report) => {
  assert.ok(report instanceof MetricReport);
  assert.strictEqual(report.type, 'meter');
  assert.strictEqual(report.name, 'test');
  assert.ok(report.time > 0);

  const [value, meta] = messages.shift();
  assert.strictEqual(report.value, value);
  assert.deepStrictEqual(report.meta, meta);
}, 3));

testMeter.mark();
testMeter.mark(123, { meta: 'extra' });

setTimeout(() => {
  testMeter.mark();
}, 200);
