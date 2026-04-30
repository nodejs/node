'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test basic consumer
const m = metrics.create('test.consumer');

const consumer = metrics.createConsumer({
  'test.consumer': { aggregation: 'sum' },
});

// Record values
m.record(1);
m.record(2);
m.record(3);

// Collect - now returns metrics array directly
const result = consumer.collect();
assert.strictEqual(result.length, 1);
assert.strictEqual(result[0].descriptor.name, 'test.consumer');
assert.strictEqual(result[0].temporality, 'cumulative');
assert.strictEqual(result[0].dataPoints[0].sum, 6);
assert.ok(result[0].timestamp > 0);

consumer.close();

// Test consumer only tracks configured metrics
const m1 = metrics.create('test.consumer.a');
const m2 = metrics.create('test.consumer.b');

const consumer2 = metrics.createConsumer({
  'test.consumer.a': { aggregation: 'sum' },
});

m1.record(10);
m2.record(20); // Should be ignored by consumer2

const result2 = consumer2.collect();
assert.strictEqual(result2.length, 1);
assert.strictEqual(result2[0].descriptor.name, 'test.consumer.a');
assert.strictEqual(result2[0].dataPoints[0].sum, 10);

consumer2.close();

// Test consumer with default aggregation (tracks all metrics)
const m3 = metrics.create('test.consumer.all');

const consumer3 = metrics.createConsumer({
  defaultAggregation: 'lastValue',
});

m3.record(42);

const result3 = consumer3.collect();
assert.ok(result3.some((s) => s.descriptor.name === 'test.consumer.all'));

consumer3.close();

// Test temporality option - delta resets between collects
const m4 = metrics.create('test.consumer.delta');

const consumer4 = metrics.createConsumer({
  'test.consumer.delta': { aggregation: 'sum', temporality: 'delta' },
});

m4.record(10);

let result4 = consumer4.collect();
assert.strictEqual(result4[0].temporality, 'delta');
assert.strictEqual(result4[0].dataPoints[0].sum, 10);
assert.strictEqual(result4[0].dataPoints[0].count, 1);

// Record more values and collect again - should NOT be cumulative
m4.record(20);
m4.record(5);

result4 = consumer4.collect();
assert.strictEqual(result4[0].dataPoints[0].sum, 25); // 20 + 5, NOT 10 + 20 + 5 = 35
assert.strictEqual(result4[0].dataPoints[0].count, 2);

// After another collect with no values, should have no data points
result4 = consumer4.collect();
assert.strictEqual(result4.length, 0); // No data points after reset

consumer4.close();

// Test cumulative temporality does NOT reset
const m5 = metrics.create('test.consumer.cumulative');

const consumer5 = metrics.createConsumer({
  'test.consumer.cumulative': { aggregation: 'sum', temporality: 'cumulative' },
});

m5.record(10);

let result5 = consumer5.collect();
assert.strictEqual(result5[0].temporality, 'cumulative');
assert.strictEqual(result5[0].dataPoints[0].sum, 10);

m5.record(20);

result5 = consumer5.collect();
assert.strictEqual(result5[0].dataPoints[0].sum, 30); // Cumulative: 10 + 20

consumer5.close();
