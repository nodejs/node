'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test multiple consumers with different configs for same metric
const m = metrics.create('test.multi', { unit: 'count' });

// Consumer 1: Sum aggregation
const consumer1 = metrics.createConsumer({
  'test.multi': { aggregation: 'sum' },
});

// Consumer 2: LastValue aggregation
const consumer2 = metrics.createConsumer({
  'test.multi': { aggregation: 'lastValue' },
});

// Consumer 3: Histogram aggregation
const consumer3 = metrics.createConsumer({
  'test.multi': { aggregation: 'histogram', boundaries: [5, 10, 20] },
});

// Record values
m.record(3);
m.record(7);
m.record(15);
m.record(25);

// Each consumer should have different interpretation
const result1 = consumer1.collect();
const result2 = consumer2.collect();
const result3 = consumer3.collect();

// Consumer 1: Sum = 50
assert.strictEqual(result1[0].dataPoints[0].sum, 50);
assert.strictEqual(result1[0].dataPoints[0].count, 4);

// Consumer 2: LastValue = 25
assert.strictEqual(result2[0].dataPoints[0].value, 25);

// Consumer 3: Histogram with 4 buckets
const hist = result3[0].dataPoints[0];
assert.strictEqual(hist.count, 4);
assert.strictEqual(hist.buckets[0].count, 1); // <= 5: value 3
assert.strictEqual(hist.buckets[1].count, 1); // <= 10: value 7
assert.strictEqual(hist.buckets[2].count, 1); // <= 20: value 15
assert.strictEqual(hist.buckets[3].count, 1); // > 20: value 25

consumer1.close();
consumer2.close();
consumer3.close();

// Test consumers only receive values after they're created
const m2 = metrics.create('test.multi.order');

m2.record(10); // Before consumer creation

const consumer4 = metrics.createConsumer({
  'test.multi.order': { aggregation: 'sum' },
});

m2.record(20); // After consumer creation

const result4 = consumer4.collect();
// Consumer should only see the value recorded after it was created
assert.strictEqual(result4[0].dataPoints[0].sum, 20);

consumer4.close();
