'use strict';

require('../common');

const assert = require('assert');
const { MetricReport } = require('node:metrics');

const report = new MetricReport('counter', 'test-counter', 123, {
  meta: 'test'
});

assert.ok(report instanceof MetricReport);
assert.strictEqual(report.type, 'counter');
assert.strictEqual(report.name, 'test-counter');
assert.strictEqual(report.value, 123);
assert.deepStrictEqual(report.meta, { meta: 'test' });
assert.ok(report.time > 0);

assert.strictEqual(report.toStatsd(), 'test-counter:123|c');
assert.strictEqual(
  report.toPrometheus(),
  `test-counter{meta="test"} 123 ${report.time}`
);
assert.strictEqual(
  report.toDogStatsd(),
  'test-counter:123|c|meta:test'
);
assert.strictEqual(
  report.toGraphite(),
  `test-counter 123 ${Math.floor(report.time / 1000)}`
);
