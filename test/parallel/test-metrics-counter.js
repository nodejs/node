'use strict';

const common = require('../common');

const assert = require('assert');
const { subscribe } = require('node:diagnostics_channel');
const { metrics } = require('node:perf_hooks');
const { createCounter, Counter, Metric, MetricReport } = metrics;

const testCounter = createCounter('test', { base: 'test' });
assert.ok(testCounter instanceof Counter);
assert.ok(testCounter instanceof Metric);
assert.strictEqual(testCounter.value, 0);

assert.strictEqual(testCounter.type, 'counter');
assert.strictEqual(testCounter.name, 'test');
assert.deepStrictEqual(testCounter.meta, { base: 'test' });
assert.strictEqual(testCounter.channelName, 'metrics:counter:test');

const messages = [
  [1, { base: 'test' }],
  [123, { base: 'test', meta: 'extra' }],
  [-1, { base: 'test' }],
  [-123, { base: 'test', meta: 'extra' }],
];

subscribe(testCounter.channelName, common.mustCall((report) => {
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
