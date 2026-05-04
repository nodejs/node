'use strict';
require('../common');
const assert = require('node:assert');
const { metrics } = require('node:perf_hooks');

// This test ensures that errors in observable callbacks are handled correctly.
// Note: Tests that trigger uncaughtException are verified in
// test-metrics-observable-exception.js

{
  // Test: Observable not calling record (means no value)
  const metric = metrics.create('test.return.null', {
    description: 'Test no record call',
    observable: () => {
      // No record() call — no value
    },
  });

  const consumer = metrics.createConsumer({
    'test.return.null': {},
  });

  // No call to record() means no data points
  const collected = consumer.collect();
  assert.strictEqual(collected.length, 0);

  consumer.close();
  metric.close();
}

{
  // Test: Observable not calling record (means no value)
  const metric = metrics.create('test.return.undefined', {
    description: 'Test no record call 2',
    observable: () => {
      // No record() call — no value
    },
  });

  const consumer = metrics.createConsumer({
    'test.return.undefined': {},
  });

  // No call to record() means no data points
  const collected = consumer.collect();
  assert.strictEqual(collected.length, 0);

  consumer.close();
  metric.close();
}

{
  // Test: Observable recording NaN (valid number type)
  const metric = metrics.create('test.return.nan', {
    description: 'Test NaN return',
    observable: (m) => {
      m.record(NaN);
    },
  });

  const consumer = metrics.createConsumer({
    'test.return.nan': {
      aggregation: 'lastValue',
    },
  });

  // NaN is a number type, so it works (though value will be NaN)
  const collected = consumer.collect();
  assert.strictEqual(collected.length, 1);
  assert(Number.isNaN(collected[0].dataPoints[0].value));

  consumer.close();
  metric.close();
}

{
  // Test: Observable recording Infinity (valid number)
  const metric = metrics.create('test.return.infinity', {
    description: 'Test Infinity return',
    observable: (m) => {
      m.record(Infinity);
    },
  });

  const consumer = metrics.createConsumer({
    'test.return.infinity': {
      aggregation: 'lastValue',
    },
  });

  // Infinity is a valid number
  const collected = consumer.collect();
  assert.strictEqual(collected.length, 1);
  assert.strictEqual(collected[0].dataPoints[0].value, Infinity);

  consumer.close();
  metric.close();
}

{
  // Test: Observable recording negative Infinity
  const metric = metrics.create('test.return.neg.infinity', {
    description: 'Test -Infinity return',
    observable: (m) => {
      m.record(-Infinity);
    },
  });

  const consumer = metrics.createConsumer({
    'test.return.neg.infinity': {
      aggregation: 'lastValue',
    },
  });

  const collected = consumer.collect();
  assert.strictEqual(collected.length, 1);
  assert.strictEqual(collected[0].dataPoints[0].value, -Infinity);

  consumer.close();
  metric.close();
}

{
  // Test: Observable recording 0
  const metric = metrics.create('test.return.zero', {
    description: 'Test zero return',
    observable: (m) => {
      m.record(0);
    },
  });

  const consumer = metrics.createConsumer({
    'test.return.zero': {
      aggregation: 'lastValue',
    },
  });

  const collected = consumer.collect();
  assert.strictEqual(collected.length, 1);
  assert.strictEqual(collected[0].dataPoints[0].value, 0);

  consumer.close();
  metric.close();
}

{
  // Test: Observable recording negative number
  const metric = metrics.create('test.return.negative', {
    description: 'Test negative return',
    observable: (m) => {
      m.record(-42);
    },
  });

  const consumer = metrics.createConsumer({
    'test.return.negative': {
      aggregation: 'lastValue',
    },
  });

  const collected = consumer.collect();
  assert.strictEqual(collected.length, 1);
  assert.strictEqual(collected[0].dataPoints[0].value, -42);

  consumer.close();
  metric.close();
}

// Note: Tests for observable callbacks that throw errors are in
// test-metrics-observable-exception.js
