'use strict';

const common = require('../common');

const assert = require('assert');
const { subscribe } = require('node:diagnostics_channel');
const { counter, Counter, Metric, MetricReport } = require('node:metrics');

const testCounter = counter('test', { base: 'test' });
assert.ok(testCounter instanceof Counter);
assert.strictEqual(testCounter.value, 0);
assert.ok(testCounter.metric instanceof Metric);

const { metric } = testCounter;
assert.strictEqual(metric.type, 'counter');
assert.strictEqual(metric.name, 'test');
assert.deepStrictEqual(metric.meta, { base: 'test' });
assert.strictEqual(metric.channelName, 'metrics:counter:test');

const messages = [
  [1, { base: 'test' }],
  [123, { base: 'test', meta: 'extra' }],
  [-1, { base: 'test' }],
  [-123, { base: 'test', meta: 'extra' }],
];

subscribe(metric.channelName, common.mustCall((report) => {
  assert.ok(report instanceof MetricReport);
  assert.strictEqual(report.type, 'counter');
  assert.strictEqual(report.name, 'test');
  assert.ok(report.time > 0);

  const [value, meta] = messages.shift();
  assert.strictEqual(report.value, value);
  assert.deepStrictEqual(report.meta, meta);
}, 4));

testCounter.increment();
testCounter.increment(123, { meta: 'extra' });
testCounter.decrement();
testCounter.decrement(123, { meta: 'extra' });
