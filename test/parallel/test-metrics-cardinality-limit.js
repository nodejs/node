'use strict';

// Test that cardinality limits prevent unbounded memory growth
// when using groupByAttributes with high-cardinality attribute values.

const common = require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Expect a warning when cardinality limit is exceeded
process.on('warning', common.mustCall((warning) => {
  assert.strictEqual(warning.name, 'MetricsWarning');
  assert.match(warning.message, /test\.cardinality\.limit/);
  assert.match(warning.message, /cardinality limit of 5/);
}, 1));

const m = metrics.create('test.cardinality.limit');

// Create a consumer with a small cardinality limit for testing
// Use delta temporality since cumulative now drops new values instead of evicting old ones
const consumer = metrics.createConsumer({
  'groupByAttributes': true,
  'test.cardinality.limit': {
    aggregation: 'sum',
    temporality: 'delta',
    cardinalityLimit: 5,
  },
});

// Record values with 10 different attribute combinations
// Only the last 5 should be retained due to cardinality limit
for (let i = 0; i < 10; i++) {
  m.record(1, { id: `item-${i}` });
}

const result = consumer.collect();
assert.strictEqual(result.length, 1);

// Should only have 5 data points due to cardinality limit
assert.strictEqual(result[0].dataPoints.length, 5);

// The retained data points should be the most recent ones (item-5 through item-9)
const ids = result[0].dataPoints.map((dp) => dp.attributes.id).sort();
assert.deepStrictEqual(ids, ['item-5', 'item-6', 'item-7', 'item-8', 'item-9']);

consumer.close();
