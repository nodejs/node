'use strict';

const common = require('../common');

const assert = require('assert');
const { subscribe } = require('node:diagnostics_channel');
const { Metric, MetricReport } = require('node:metrics');

const metric = new Metric('counter', 'test-counter', { base: 'test' });

assert.ok(metric instanceof Metric);
assert.strictEqual(metric.type, 'counter');
assert.strictEqual(metric.name, 'test-counter');
assert.deepStrictEqual(metric.meta, { base: 'test' });
assert.strictEqual(metric.channelName, 'metrics:counter:test-counter');

subscribe(metric.channelName, common.mustCall((report) => {
  assert.ok(report instanceof MetricReport);
  assert.strictEqual(report.type, 'counter');
  assert.strictEqual(report.name, 'test-counter');
  assert.ok(report.time > 0);

  assert.strictEqual(report.value, 123);
  assert.deepStrictEqual(report.meta, { base: 'test', meta: 'test' });
}));

metric.report(123, { meta: 'test' });
