'use strict';

// Test that observable metrics maintain consumer isolation.
// Each consumer should only see values when IT calls collect(),
// not when other consumers collect.

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

let callCount = 0;
// Metric created to register the observable - consumers subscribe to it
metrics.create('test.observable.isolation', {
  observable: (metric) => {
    callCount++;
    metric.record(callCount * 10);
  },
});

// Create two consumers for the same observable metric
const consumer1 = metrics.createConsumer({
  'test.observable.isolation': { aggregation: 'lastValue' },
});

const consumer2 = metrics.createConsumer({
  'test.observable.isolation': { aggregation: 'lastValue' },
});

// Initial state - no collects yet
assert.strictEqual(callCount, 0);

// Consumer1 collects - observable should be called once for consumer1's subscriber
const result1 = consumer1.collect();
assert.strictEqual(callCount, 1);
assert.strictEqual(result1.length, 1);
assert.strictEqual(result1[0].dataPoints[0].value, 10);

// Consumer2 collects - observable should be called once for consumer2's subscriber
// NOT twice (which would happen if consumer1's collect triggered consumer2's subscriber)
const result2 = consumer2.collect();
assert.strictEqual(callCount, 2);
assert.strictEqual(result2.length, 1);
assert.strictEqual(result2[0].dataPoints[0].value, 20);

// Consumer1 collects again - should only increment by 1
const result3 = consumer1.collect();
assert.strictEqual(callCount, 3);
assert.strictEqual(result3[0].dataPoints[0].value, 30);

// Consumer2 collects again - should only increment by 1
const result4 = consumer2.collect();
assert.strictEqual(callCount, 4);
assert.strictEqual(result4[0].dataPoints[0].value, 40);

consumer1.close();
consumer2.close();
