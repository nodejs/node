'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test sum aggregator
const m = metrics.create('test.sum');

const consumer = metrics.createConsumer({
  'test.sum': { aggregation: 'sum' },
});

m.record(10);
m.record(20);
m.record(30);

let result = consumer.collect();
assert.strictEqual(result[0].dataPoints[0].sum, 60);
assert.strictEqual(result[0].dataPoints[0].count, 3);

consumer.close();

// Test monotonic sum (ignores negative values)
const m2 = metrics.create('test.sum.monotonic');

const consumer2 = metrics.createConsumer({
  'test.sum.monotonic': { aggregation: 'sum', monotonic: true },
});

m2.record(10);
m2.record(-5);  // Should be ignored
m2.record(20);
m2.record(-10); // Should be ignored

result = consumer2.collect();
assert.strictEqual(result[0].dataPoints[0].sum, 30); // 10 + 20
assert.strictEqual(result[0].dataPoints[0].count, 2); // Only positive values counted

consumer2.close();

// Test non-monotonic sum (includes negative values)
const m3 = metrics.create('test.sum.nonmonotonic');

const consumer3 = metrics.createConsumer({
  'test.sum.nonmonotonic': { aggregation: 'sum', monotonic: false },
});

m3.record(10);
m3.record(-5);
m3.record(20);

result = consumer3.collect();
assert.strictEqual(result[0].dataPoints[0].sum, 25); // 10 - 5 + 20
assert.strictEqual(result[0].dataPoints[0].count, 3);

consumer3.close();

// Test cumulative temporality (default)
const m4 = metrics.create('test.sum.cumulative');

const consumer4 = metrics.createConsumer({
  'test.sum.cumulative': { aggregation: 'sum', temporality: 'cumulative' },
});

m4.record(10);
result = consumer4.collect();
assert.strictEqual(result[0].dataPoints[0].sum, 10);
assert.strictEqual(result[0].temporality, 'cumulative');

m4.record(20);
result = consumer4.collect();
assert.strictEqual(result[0].dataPoints[0].sum, 30); // Cumulative

consumer4.close();
