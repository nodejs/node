'use strict';

const common = require('../common');

const assert = require('assert');
const { subscribe } = require('node:diagnostics_channel');
const { periodicGauge, PeriodicGauge, Metric, MetricReport } = require('node:metrics');

// NOTE: If this test is flaky, tune the interval to give more leeway to the timing
const interval = 50;
const values = [ 1, 5, 10, 4, 6 ];

const testPeriodicGauge = periodicGauge('test', 10, () => {
  const value = values.shift();
  if (!values.length) {
    testPeriodicGauge.stop();
  } else {
    testPeriodicGauge.interval = interval;
    testPeriodicGauge.ref();
    assert.strictEqual(testPeriodicGauge.interval, interval);
  }
  return value;
}, { base: 'test' });

// Keep the loop alive
testPeriodicGauge.ref();

assert.ok(testPeriodicGauge instanceof PeriodicGauge);
assert.strictEqual(testPeriodicGauge.value, 0);
assert.strictEqual(testPeriodicGauge.interval, 10);
assert.ok(testPeriodicGauge.metric instanceof Metric);

const { metric } = testPeriodicGauge;
assert.strictEqual(metric.type, 'periodicGauge');
assert.strictEqual(metric.name, 'test');
assert.deepStrictEqual(metric.meta, { base: 'test' });
assert.strictEqual(metric.channelName, 'metrics:periodicGauge:test');

const messages = values.map((v) => [v, { base: 'test' }]);

subscribe(metric.channelName, common.mustCall((report) => {
  assert.ok(report instanceof MetricReport);
  assert.strictEqual(report.type, 'periodicGauge');
  assert.strictEqual(report.name, 'test');
  assert.ok(report.time > 0);

  const [value, meta] = messages.shift();
  assert.strictEqual(report.value, value);
  assert.deepStrictEqual(report.meta, meta);
}, values.length));
