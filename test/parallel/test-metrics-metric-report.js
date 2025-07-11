'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('node:perf_hooks');
const { MetricReport } = metrics;

const report = new MetricReport('counter', 'test-counter', 123, {
  meta: 'test'
});

assert.ok(report instanceof MetricReport);
assert.strictEqual(report.type, 'counter');
assert.strictEqual(report.name, 'test-counter');
assert.strictEqual(report.value, 123);
assert.deepStrictEqual(report.meta, { meta: 'test' });
assert.ok(report.time > 0);
