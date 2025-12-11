'use strict';

// Test that consumers properly subscribe to metrics created AFTER the consumer.
// This tests the onMetricCreated callback path.

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test 1: Consumer waiting for specific metric that doesn't exist yet
{
  // Create consumer BEFORE the metric exists
  const consumer = metrics.createConsumer({
    'test.late.specific': { aggregation: 'sum' },
  });

  // Collect before metric exists - should be empty
  let result = consumer.collect();
  assert.strictEqual(result.length, 0);

  // Now create the metric
  const m = metrics.create('test.late.specific');

  // Record a value
  m.record(42);

  // Collect - should now have the value
  result = consumer.collect();
  assert.strictEqual(result.length, 1);
  assert.strictEqual(result[0].descriptor.name, 'test.late.specific');
  assert.strictEqual(result[0].dataPoints[0].sum, 42);

  consumer.close();
}

// Test 2: Wildcard consumer receives metrics created after it
{
  // Create wildcard consumer
  const consumer = metrics.createConsumer({
    defaultAggregation: 'sum',
  });

  // Create a metric AFTER the consumer
  const m = metrics.create('test.late.wildcard');
  m.record(100);

  // Collect - should have the new metric
  const result = consumer.collect();
  const found = result.find((s) => s.descriptor.name === 'test.late.wildcard');
  assert.ok(found, 'Wildcard consumer should receive metrics created after it');
  assert.strictEqual(found.dataPoints[0].sum, 100);

  consumer.close();
}

// Test 3: Multiple consumers waiting for the same metric
{
  const consumer1 = metrics.createConsumer({
    'test.late.multi': { aggregation: 'sum' },
  });
  const consumer2 = metrics.createConsumer({
    'test.late.multi': { aggregation: 'lastValue' },
  });

  // Create the metric after both consumers
  const m = metrics.create('test.late.multi');
  m.record(50);
  m.record(75);

  // Both consumers should receive the values with their own aggregation
  const result1 = consumer1.collect();
  const result2 = consumer2.collect();

  assert.strictEqual(result1[0].dataPoints[0].sum, 125); // 50 + 75
  assert.strictEqual(result2[0].dataPoints[0].value, 75); // last value

  consumer1.close();
  consumer2.close();
}

// Test 4: Consumer created for non-existent metric, then closed before metric exists
{
  const consumer = metrics.createConsumer({
    'test.late.never': { aggregation: 'sum' },
  });

  // Close consumer before metric is created
  consumer.close();

  // Now create the metric - should not crash
  const m = metrics.create('test.late.never');
  m.record(100);

  // No consumer to verify, just ensure no crash
}

// Test 5: Observable metric created after consumer
{
  let observableValue = 42;

  const consumer = metrics.createConsumer({
    'test.late.observable': { aggregation: 'lastValue' },
  });

  // Create observable metric after consumer
  metrics.create('test.late.observable', {
    observable: (metric) => { metric.record(observableValue); },
  });

  // Collect should sample the observable
  let result = consumer.collect();
  assert.strictEqual(result.length, 1);
  assert.strictEqual(result[0].dataPoints[0].value, 42);

  // Update and collect again
  observableValue = 100;
  result = consumer.collect();
  assert.strictEqual(result[0].dataPoints[0].value, 100);

  consumer.close();
}
