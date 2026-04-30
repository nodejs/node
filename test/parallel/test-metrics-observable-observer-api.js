'use strict';
require('../common');
const assert = require('node:assert');
const { metrics } = require('node:perf_hooks');

// This test ensures that the facade record() multi-value API works correctly
// for observable metrics, including validation and aggregation behavior.

{
  // Test: Multi-value reporting via facade record() with different attributes
  const metric = metrics.create('test.multi.value', {
    description: 'Test multi-value observe API',
    observable: (metric) => {
      metric.record(10, { key: 'a' });
      metric.record(20, { key: 'b' });
      metric.record(30, { key: 'c' });
    },
  });

  const consumer = metrics.createConsumer({
    groupByAttributes: true,
    metrics: {
      'test.multi.value': {
        aggregation: 'lastValue',
      },
    },
  });

  const collected = consumer.collect();
  consumer.close();
  metric.close();

  assert.strictEqual(collected.length, 1);
  const points = collected[0].dataPoints;
  assert.strictEqual(points.length, 3);

  const values = points.map((p) => ({ value: p.value, key: p.attributes.key }))
    .sort((a, b) => a.key.localeCompare(b.key));

  assert.deepStrictEqual(values, [
    { value: 10, key: 'a' },
    { value: 20, key: 'b' },
    { value: 30, key: 'c' },
  ]);
}

{
  // Test: facade record() with bigint values and attributes
  const metric = metrics.create('test.bigint.observe', {
    description: 'Test bigint values',
    observable: (metric) => {
      metric.record(100n, { type: 'small' });
      metric.record(9007199254740991n, { type: 'large' });
    },
  });

  const consumer = metrics.createConsumer({
    groupByAttributes: true,
    metrics: {
      'test.bigint.observe': {},
    },
  });

  const collected = consumer.collect();
  consumer.close();
  metric.close();

  assert.strictEqual(collected.length, 1);
  const points = collected[0].dataPoints;
  assert.strictEqual(points.length, 2);

  const largeValue = points.find((p) => p.attributes.type === 'large');
  assert.strictEqual(largeValue.sum, 9007199254740991n);
}

{
  // Test: facade record() called multiple times with same attributes aggregates
  const metric = metrics.create('test.same.attrs', {
    description: 'Test same attributes',
    observable: (metric) => {
      metric.record(5, { env: 'prod' });
      metric.record(10, { env: 'prod' });
      metric.record(15, { env: 'prod' });
    },
  });

  const consumer = metrics.createConsumer({
    groupByAttributes: true,
    metrics: {
      'test.same.attrs': {},
    },
  });

  const collected = consumer.collect();
  consumer.close();
  metric.close();

  assert.strictEqual(collected.length, 1);
  const points = collected[0].dataPoints;
  // Should aggregate to single point with same attributes
  assert.strictEqual(points.length, 1);
  assert.strictEqual(points[0].sum, 30);
  assert.strictEqual(points[0].count, 3);
  assert.deepStrictEqual(points[0].attributes, { env: 'prod' });
}

{
  // Test: Empty observable (neither return nor observe calls)
  const metric = metrics.create('test.empty.observable', {
    description: 'Test empty observable',
    observable: () => {
      // No observe calls, no return value
    },
  });

  const consumer = metrics.createConsumer({
    'test.empty.observable': {},
  });

  const collected = consumer.collect();
  consumer.close();
  metric.close();

  // Should have no data when observable produces nothing
  assert.strictEqual(collected.length, 0);
}

{
  // Test: facade record() with histogram aggregation
  const metric = metrics.create('test.histogram.observe', {
    description: 'Test histogram with observe',
    observable: (metric) => {
      metric.record(5, { bucket: 'low' });
      metric.record(15, { bucket: 'mid' });
      metric.record(25, { bucket: 'high' });
    },
  });

  const consumer = metrics.createConsumer({
    groupByAttributes: true,
    metrics: {
      'test.histogram.observe': {
        aggregation: 'histogram',
      },
    },
  });

  const collected = consumer.collect();
  consumer.close();
  metric.close();

  assert.strictEqual(collected.length, 1);
  const points = collected[0].dataPoints;
  assert.strictEqual(points.length, 3);

  // Verify each point has histogram-specific fields
  points.forEach((point) => {
    assert(Array.isArray(point.buckets));
    assert.strictEqual(typeof point.count, 'number');
    assert.strictEqual(typeof point.sum, 'number');
    assert.strictEqual(typeof point.min, 'number');
    assert.strictEqual(typeof point.max, 'number');
  });
}

{
  // Test: facade record() with no attributes defaults to empty object
  const metric = metrics.create('test.no.attrs', {
    description: 'Test no attributes',
    observable: (metric) => {
      metric.record(42);
    },
  });

  const consumer = metrics.createConsumer({
    'test.no.attrs': {},
  });

  const collected = consumer.collect();
  consumer.close();
  metric.close();

  assert.strictEqual(collected.length, 1);
  assert.strictEqual(collected[0].dataPoints.length, 1);
  assert.strictEqual(collected[0].dataPoints[0].sum, 42);
  // Attributes should be empty (kEmptyObject)
  assert.strictEqual(Object.keys(collected[0].dataPoints[0].attributes).length, 0);
}

{
  // Test: Without groupByAttributes, all values aggregate together
  const metric = metrics.create('test.no.grouping', {
    description: 'Test without groupByAttributes',
    observable: (metric) => {
      metric.record(10, { key: 'a' });
      metric.record(20, { key: 'b' });
      metric.record(30, { key: 'c' });
    },
  });

  const consumer = metrics.createConsumer({
    // No groupByAttributes
    'test.no.grouping': {},
  });

  const collected = consumer.collect();
  consumer.close();
  metric.close();

  assert.strictEqual(collected.length, 1);
  assert.strictEqual(collected[0].dataPoints.length, 1);
  assert.strictEqual(collected[0].dataPoints[0].sum, 60);
  assert.strictEqual(collected[0].dataPoints[0].count, 3);
  // Attributes are ignored without groupByAttributes
  assert.strictEqual(Object.keys(collected[0].dataPoints[0].attributes).length, 0);
}
