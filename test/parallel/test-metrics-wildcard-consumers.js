'use strict';

// Test multiple wildcard consumers (consumers with no specific metrics configured).
// Each should independently aggregate all metrics.

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Create multiple wildcard consumers with different configurations
const consumer1 = metrics.createConsumer({
  defaultAggregation: 'sum',
  defaultTemporality: 'cumulative',
});

const consumer2 = metrics.createConsumer({
  defaultAggregation: 'lastValue',
  defaultTemporality: 'cumulative',
});

const consumer3 = metrics.createConsumer({
  defaultAggregation: 'sum',
  defaultTemporality: 'delta',
});

// Create a metric and record values
const m = metrics.create('test.wildcard.metric');
m.record(10);
m.record(20);
m.record(30);

// All consumers should see the metric
const result1 = consumer1.collect();
const result2 = consumer2.collect();
const result3 = consumer3.collect();

// Find our metric in each result
const find = (result, name) => result.find((s) => s.descriptor.name === name);

const metric1 = find(result1, 'test.wildcard.metric');
const metric2 = find(result2, 'test.wildcard.metric');
const metric3 = find(result3, 'test.wildcard.metric');

assert.ok(metric1, 'Consumer 1 should have the metric');
assert.ok(metric2, 'Consumer 2 should have the metric');
assert.ok(metric3, 'Consumer 3 should have the metric');

// Consumer 1: Sum aggregation
assert.strictEqual(metric1.dataPoints[0].sum, 60);
assert.strictEqual(metric1.temporality, 'cumulative');

// Consumer 2: LastValue aggregation
assert.strictEqual(metric2.dataPoints[0].value, 30);
assert.strictEqual(metric2.temporality, 'cumulative');

// Consumer 3: Sum with delta temporality
assert.strictEqual(metric3.dataPoints[0].sum, 60);
assert.strictEqual(metric3.temporality, 'delta');

// Test delta reset for consumer3
m.record(5);

const result1b = consumer1.collect();
const result3b = consumer3.collect();

const metric1b = find(result1b, 'test.wildcard.metric');
const metric3b = find(result3b, 'test.wildcard.metric');

// Consumer 1 (cumulative): Should be 65 (60 + 5)
assert.strictEqual(metric1b.dataPoints[0].sum, 65);

// Consumer 3 (delta): Should be 5 (only new value since last collect)
assert.strictEqual(metric3b.dataPoints[0].sum, 5);

// Test that new metrics are picked up by all wildcard consumers
const m2 = metrics.create('test.wildcard.new');
m2.record(100);

const result1c = consumer1.collect();
const result2c = consumer2.collect();
const result3c = consumer3.collect();

const newMetric1 = find(result1c, 'test.wildcard.new');
const newMetric2 = find(result2c, 'test.wildcard.new');
const newMetric3 = find(result3c, 'test.wildcard.new');

assert.ok(newMetric1, 'Consumer 1 should see new metric');
assert.ok(newMetric2, 'Consumer 2 should see new metric');
assert.ok(newMetric3, 'Consumer 3 should see new metric');

assert.strictEqual(newMetric1.dataPoints[0].sum, 100);
assert.strictEqual(newMetric2.dataPoints[0].value, 100);
assert.strictEqual(newMetric3.dataPoints[0].sum, 100);

// Test closing one consumer doesn't affect others
consumer1.close();

m.record(15);

const result2d = consumer2.collect();
const result3d = consumer3.collect();

const metric2d = find(result2d, 'test.wildcard.metric');
const metric3d = find(result3d, 'test.wildcard.metric');

assert.strictEqual(metric2d.dataPoints[0].value, 15);
assert.strictEqual(metric3d.dataPoints[0].sum, 15);

consumer2.close();
consumer3.close();
