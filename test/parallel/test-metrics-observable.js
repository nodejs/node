'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test observable metric
let gaugeValue = 42;
const m = metrics.create('test.observable', {
  unit: '{count}',
  description: 'Observable test',
  observable: (metric) => { metric.record(gaugeValue); },
});

assert.strictEqual(m.isObservable, true);

// Consumer for observable
const consumer = metrics.createConsumer({
  'test.observable': { aggregation: 'lastValue' },
});

// First collect
let result = consumer.collect();
assert.strictEqual(result.length, 1);
assert.strictEqual(result[0].dataPoints[0].value, 42);

// Update value and collect again
gaugeValue = 100;
result = consumer.collect();
assert.strictEqual(result[0].dataPoints[0].value, 100);

// Value changes between collects
gaugeValue = 0;
result = consumer.collect();
assert.strictEqual(result[0].dataPoints[0].value, 0);

consumer.close();

// Test observable that returns null/undefined (should not record)
let maybeValue = null;
metrics.create('test.observable.null', {
  observable: (metric) => {
    if (maybeValue != null) metric.record(maybeValue);
  },
});

const consumer2 = metrics.createConsumer({
  'test.observable.null': { aggregation: 'lastValue' },
});

// Collect when observable returns null - should have no data points
result = consumer2.collect();
assert.strictEqual(result.length, 0);

// Now return a value
maybeValue = 50;
result = consumer2.collect();
assert.strictEqual(result.length, 1);
assert.strictEqual(result[0].dataPoints[0].value, 50);

consumer2.close();

// Test validation - observable must be a function
assert.throws(() => metrics.create('test.bad.observable', { observable: 'not a function' }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
