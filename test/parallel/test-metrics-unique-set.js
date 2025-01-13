'use strict';

const common = require('../common');

const assert = require('assert');
const { subscribe } = require('node:diagnostics_channel');
const { uniqueSet, UniqueSet, Metric, MetricReport } = require('node:metrics');

const testUniqueSet = uniqueSet('test', { base: 'test' });
assert.ok(testUniqueSet instanceof UniqueSet);
assert.strictEqual(testUniqueSet.value, 0);
assert.ok(testUniqueSet.metric instanceof Metric);

const { metric } = testUniqueSet;
assert.strictEqual(metric.type, 'uniqueSet');
assert.strictEqual(metric.name, 'test');
assert.deepStrictEqual(metric.meta, { base: 'test' });
assert.strictEqual(metric.channelName, 'metrics:uniqueSet:test');

const messages = [
  [1, { base: 'test', meta: 'foo' }],
  [2, { base: 'test', meta: 'baz' }],
];

subscribe(metric.channelName, common.mustCall((report) => {
  assert.ok(report instanceof MetricReport);
  assert.strictEqual(report.type, 'uniqueSet');
  assert.strictEqual(report.name, 'test');
  assert.ok(report.time > 0);

  const [value, meta] = messages.shift();
  assert.strictEqual(report.value, value);
  assert.deepStrictEqual(report.meta, meta);
}, 2));

const foo = { foo: 'bar' };
const baz = { baz: 'buz' };

assert.strictEqual(testUniqueSet.count, 0);

testUniqueSet.add(foo, { meta: 'foo' });
assert.strictEqual(testUniqueSet.count, 1);

testUniqueSet.add(foo, { meta: 'should not trigger or report!' });
assert.strictEqual(testUniqueSet.count, 1);

testUniqueSet.add(baz, { meta: 'baz' });
assert.strictEqual(testUniqueSet.count, 2);
