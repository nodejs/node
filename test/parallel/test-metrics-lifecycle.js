'use strict';
require('../common');
const assert = require('node:assert');
const { metrics } = require('node:perf_hooks');

// This test ensures that metric lifecycle methods (close, isClosed) work correctly
// and that consumers are properly notified when metrics are closed.

{
  // Test: metric.close() removes metric from registry
  const metric = metrics.create('test.close.basic', {
    description: 'Test close basic behavior',
  });

  assert.strictEqual(metric.isClosed, false);

  const consumer = metrics.createConsumer({
    'test.close.basic': {},
  });

  metric.record(10);
  const before = consumer.collect();
  assert.strictEqual(before.length, 1);

  metric.close();
  assert.strictEqual(metric.isClosed, true);

  const after = consumer.collect();
  assert.strictEqual(after.length, 0);

  consumer.close();
}

{
  // Test: Double close is idempotent
  const metric = metrics.create('test.double.close', {
    description: 'Test double close',
  });

  assert.strictEqual(metric.isClosed, false);
  metric.close();
  assert.strictEqual(metric.isClosed, true);

  // Second close should not throw
  metric.close();
  assert.strictEqual(metric.isClosed, true);
}

{
  // Test: Recording to closed metric (values ignored, no error)
  const metric = metrics.create('test.record.after.close', {
    description: 'Test record after close',
  });

  const consumer = metrics.createConsumer({
    'test.record.after.close': {},
  });

  metric.record(10);
  metric.close();

  // Recording after close should not throw
  metric.record(20);
  metric.record(30);

  const collected = consumer.collect();
  // Metric was closed, so nothing should be collected
  assert.strictEqual(collected.length, 0);

  consumer.close();
}

{
  // Test: Starting timer on closed metric
  const metric = metrics.create('test.timer.after.close', {
    description: 'Test timer after close',
  });

  const consumer = metrics.createConsumer({
    'test.timer.after.close': {},
  });

  metric.close();

  // Timer should work but recording will be ignored
  const timer = metric.startTimer();
  timer.stop();

  const collected = consumer.collect();
  assert.strictEqual(collected.length, 0);

  consumer.close();
}

{
  // Test: Sampling observable after metric closed
  const metric = metrics.create('test.observable.after.close', {
    description: 'Test observable after close',
    observable: (m) => { m.record(100); },
  });

  const consumer = metrics.createConsumer({
    'test.observable.after.close': {
      aggregation: 'lastValue',
    },
  });

  const before = consumer.collect();
  assert.strictEqual(before.length, 1);
  assert.strictEqual(before[0].dataPoints[0].value, 100);

  metric.close();

  const after = consumer.collect();
  // Metric is closed, should not be collected
  assert.strictEqual(after.length, 0);

  consumer.close();
}

{
  // Test: Creating new metric with same name after close
  const metric1 = metrics.create('test.recreate', {
    description: 'Original metric',
  });

  const consumer = metrics.createConsumer({
    'test.recreate': {},
  });

  metric1.record(10);
  const first = consumer.collect();
  assert.strictEqual(first.length, 1);
  assert.strictEqual(first[0].dataPoints[0].sum, 10);

  metric1.close();

  // Create new consumer for the new metric
  const consumer2 = metrics.createConsumer({
    'test.recreate': {},
  });

  // Create new metric with same name
  const metric2 = metrics.create('test.recreate', {
    description: 'New metric instance',
  });

  // Should be a fresh instance
  assert.notStrictEqual(metric1, metric2);
  assert.strictEqual(metric1.isClosed, true);
  assert.strictEqual(metric2.isClosed, false);

  metric2.record(50);

  const second = consumer2.collect();
  assert.strictEqual(second.length, 1);
  // Should have only the new recording
  assert.strictEqual(second[0].dataPoints[0].sum, 50);

  metric2.close();
  consumer.close();
  consumer2.close();
}

{
  // Test: Consumer notified when metric closes
  let onClosedCalled = false;
  let closedMetricName = null;

  const metric = metrics.create('test.consumer.notification', {
    description: 'Test consumer notification on close',
  });

  const consumer = metrics.createConsumer({
    'test.consumer.notification': {},
  });

  // Override onMetricClosed to capture notification
  const originalOnClosed = consumer.onMetricClosed;
  consumer.onMetricClosed = function(closedMetric) {
    onClosedCalled = true;
    closedMetricName = closedMetric.descriptor.name;
    if (originalOnClosed) {
      originalOnClosed.call(this, closedMetric);
    }
  };

  assert.strictEqual(onClosedCalled, false);

  metric.close();

  // Consumer should have been notified
  assert.strictEqual(onClosedCalled, true);
  assert.strictEqual(closedMetricName, 'test.consumer.notification');

  consumer.close();
}

{
  // Test: Multiple consumers notified when metric closes
  const metric = metrics.create('test.multi.consumer.close', {
    description: 'Test multiple consumers on close',
  });

  let consumer1Notified = false;
  let consumer2Notified = false;

  const consumer1 = metrics.createConsumer({
    'test.multi.consumer.close': {},
  });
  const consumer2 = metrics.createConsumer({
    'test.multi.consumer.close': {},
  });

  const origOnClosed1 = consumer1.onMetricClosed;
  consumer1.onMetricClosed = function(closedMetric) {
    consumer1Notified = true;
    if (origOnClosed1) {
      origOnClosed1.call(this, closedMetric);
    }
  };

  const origOnClosed2 = consumer2.onMetricClosed;
  consumer2.onMetricClosed = function(closedMetric) {
    consumer2Notified = true;
    if (origOnClosed2) {
      origOnClosed2.call(this, closedMetric);
    }
  };

  metric.close();

  assert.strictEqual(consumer1Notified, true);
  assert.strictEqual(consumer2Notified, true);

  consumer1.close();
  consumer2.close();
}

{
  // Test: Consumer correctly cleans up after metric closes
  const metric = metrics.create('test.cleanup.after.close', {
    description: 'Test cleanup after close',
  });

  const consumer = metrics.createConsumer({
    'test.cleanup.after.close': {},
  });

  metric.record(10);
  const before = consumer.collect();
  assert.strictEqual(before.length, 1);

  metric.close();

  // Collect after close should return empty array
  const after = consumer.collect();
  assert.strictEqual(after.length, 0);

  // Multiple collects should still work
  const after2 = consumer.collect();
  assert.strictEqual(after2.length, 0);

  consumer.close();
}
