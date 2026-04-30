'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test histogram aggregator with custom boundaries
const m = metrics.create('test.histogram', { unit: 'ms' });

const consumer = metrics.createConsumer({
  'test.histogram': {
    aggregation: 'histogram',
    boundaries: [10, 50, 100, 500],
  },
});

// Record values across different buckets
m.record(5);    // bucket: <= 10
m.record(25);   // bucket: <= 50
m.record(75);   // bucket: <= 100
m.record(200);  // bucket: <= 500
m.record(750);  // bucket: > 500 (Infinity)

const result = consumer.collect();
const hist = result[0].dataPoints[0];

assert.strictEqual(hist.count, 5);
assert.strictEqual(hist.sum, 1055);
assert.strictEqual(hist.min, 5);
assert.strictEqual(hist.max, 750);

// Check bucket counts
assert.strictEqual(hist.buckets.length, 5); // 4 boundaries + 1 overflow
assert.strictEqual(hist.buckets[0].le, 10);
assert.strictEqual(hist.buckets[0].count, 1);
assert.strictEqual(hist.buckets[1].le, 50);
assert.strictEqual(hist.buckets[1].count, 1);
assert.strictEqual(hist.buckets[2].le, 100);
assert.strictEqual(hist.buckets[2].count, 1);
assert.strictEqual(hist.buckets[3].le, 500);
assert.strictEqual(hist.buckets[3].count, 1);
assert.strictEqual(hist.buckets[4].count, 1); // Infinity bucket

consumer.close();

// Test histogram with default boundaries
const m2 = metrics.create('test.histogram.default');

const consumer2 = metrics.createConsumer({
  'test.histogram.default': { aggregation: 'histogram' },
});

m2.record(5);
m2.record(25);
m2.record(75);

const result2 = consumer2.collect();
const hist2 = result2[0].dataPoints[0];
assert.strictEqual(hist2.count, 3);
assert.ok(hist2.buckets.length > 0);

consumer2.close();

// Test empty histogram (no recordings)
metrics.create('test.histogram.empty');

const consumer3 = metrics.createConsumer({
  'test.histogram.empty': { aggregation: 'histogram' },
});

// Don't record anything, then collect
// This should not create a data point since there's no data
const result3 = consumer3.collect();
assert.strictEqual(result3.length, 0);

consumer3.close();
