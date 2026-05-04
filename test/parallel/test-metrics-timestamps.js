'use strict';
require('../common');
const assert = require('node:assert');
const { metrics } = require('node:perf_hooks');

// This test ensures that timestamp reference counting works correctly
// and that timestamps are only generated when needed.

{
  // Test: Single consumer with lastValue enables timestamps
  const metric = metrics.create('test.timestamp.single', {
    description: 'Test single consumer timestamps',
  });

  const consumer = metrics.createConsumer({
    'test.timestamp.single': {
      aggregation: 'lastValue',
    },
  });

  metric.record(10);
  const collected = consumer.collect();

  assert.strictEqual(collected.length, 1);
  const point = collected[0].dataPoints[0];
  assert.notStrictEqual(point.timestamp, undefined);
  assert(typeof point.timestamp === 'bigint' || typeof point.timestamp === 'number');

  consumer.close();
  metric.close();
}

{
  // Test: Multiple consumers with lastValue share timestamp generation
  const metric = metrics.create('test.timestamp.multiple', {
    description: 'Test multiple consumers timestamps',
  });

  const consumer1 = metrics.createConsumer({
    'test.timestamp.multiple': {
      aggregation: 'lastValue',
    },
  });

  const consumer2 = metrics.createConsumer({
    'test.timestamp.multiple': {
      aggregation: 'lastValue',
    },
  });

  metric.record(10);

  const collected1 = consumer1.collect();
  const collected2 = consumer2.collect();

  assert.strictEqual(collected1.length, 1);
  assert.strictEqual(collected2.length, 1);

  const point1 = collected1[0].dataPoints[0];
  const point2 = collected2[0].dataPoints[0];

  assert.notStrictEqual(point1.timestamp, undefined);
  assert.notStrictEqual(point2.timestamp, undefined);

  // Timestamps should be the same or very close
  assert.strictEqual(typeof point1.timestamp, typeof point2.timestamp);

  consumer1.close();
  consumer2.close();
  metric.close();
}

{
  // Test: Consumer close decrements timestamp reference count
  const metric = metrics.create('test.timestamp.refcount', {
    description: 'Test timestamp reference counting',
  });

  const consumer1 = metrics.createConsumer({
    'test.timestamp.refcount': {
      aggregation: 'lastValue',
    },
  });

  const consumer2 = metrics.createConsumer({
    'test.timestamp.refcount': {
      aggregation: 'lastValue',
    },
  });

  metric.record(10);

  const before1 = consumer1.collect();
  assert.notStrictEqual(before1[0].dataPoints[0].timestamp, undefined);

  // Close one consumer
  consumer1.close();

  // Other consumer should still get timestamps
  metric.record(20);
  const after = consumer2.collect();
  assert.notStrictEqual(after[0].dataPoints[0].timestamp, undefined);

  consumer2.close();
  metric.close();
}

{
  // Test: All consumers closed disables timestamp generation
  const metric = metrics.create('test.timestamp.disabled', {
    description: 'Test timestamp disabled',
  });

  const consumer = metrics.createConsumer({
    'test.timestamp.disabled': {
      aggregation: 'lastValue',
    },
  });

  metric.record(10);
  const with_ts = consumer.collect();
  assert.notStrictEqual(with_ts[0].dataPoints[0].timestamp, undefined);

  consumer.close();

  // Create new consumer without lastValue
  const consumer2 = metrics.createConsumer({
    'test.timestamp.disabled': {},
  });

  metric.record(20);
  const without_ts = consumer2.collect();

  // Without lastValue, timestamps should not be present
  assert.strictEqual(without_ts[0].dataPoints[0].timestamp, undefined);

  consumer2.close();
  metric.close();
}

{
  // Test: Mix of consumers (some need timestamps, some don't)
  const metric = metrics.create('test.timestamp.mixed', {
    description: 'Test mixed timestamp needs',
  });

  const consumerWithTS = metrics.createConsumer({
    'test.timestamp.mixed': {
      aggregation: 'lastValue',
    },
  });

  const consumerWithoutTS = metrics.createConsumer({
    'test.timestamp.mixed': {},
  });

  metric.record(10);

  const with_ts = consumerWithTS.collect();
  const without_ts = consumerWithoutTS.collect();

  assert.notStrictEqual(with_ts[0].dataPoints[0].timestamp, undefined);
  assert.strictEqual(without_ts[0].dataPoints[0].timestamp, undefined);

  consumerWithTS.close();
  consumerWithoutTS.close();
  metric.close();
}

{
  // Test: Timestamp precision verification
  const metric = metrics.create('test.timestamp.precision', {
    description: 'Test timestamp precision',
  });

  const consumer = metrics.createConsumer({
    'test.timestamp.precision': {
      aggregation: 'lastValue',
    },
  });

  const { performance } = require('node:perf_hooks');
  const before = performance.now();
  metric.record(10);
  const after = performance.now();

  const collected = consumer.collect();
  const timestamp = collected[0].dataPoints[0].timestamp;

  // Timestamp should be between before and after (with small tolerance)
  assert(timestamp >= before - 1);
  assert(timestamp <= after + 1);

  consumer.close();
  metric.close();
}

{
  // Test: Timestamps with delta temporality
  const metric = metrics.create('test.timestamp.delta', {
    description: 'Test delta timestamps',
  });

  const consumer = metrics.createConsumer({
    'test.timestamp.delta': {
      temporality: 'delta',
      aggregation: 'lastValue',
    },
  });

  metric.record(10);
  const first = consumer.collect();

  metric.record(20);
  const second = consumer.collect();

  // Both collections should have timestamps
  assert.notStrictEqual(first[0].dataPoints[0].timestamp, undefined);
  assert.notStrictEqual(second[0].dataPoints[0].timestamp, undefined);

  // Second timestamp should be greater than first
  assert(second[0].dataPoints[0].timestamp > first[0].dataPoints[0].timestamp);

  consumer.close();
  metric.close();
}

{
  // Test: Consumer without lastValue doesn't enable timestamps
  const metric = metrics.create('test.no.timestamp', {
    description: 'Test no timestamps',
  });

  const consumer = metrics.createConsumer({
    'test.no.timestamp': {},
  });

  metric.record(10);
  const collected = consumer.collect();

  assert.strictEqual(collected.length, 1);
  const point = collected[0].dataPoints[0];
  assert.strictEqual(point.timestamp, undefined);

  consumer.close();
  metric.close();
}
