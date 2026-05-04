'use strict';

// Test that observable callback exceptions are surfaced via uncaughtException
// (like diagnostics_channel) rather than being silently swallowed.
// The collection should continue despite the error.

const common = require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test observable that throws
let throwOnCall = true;
metrics.create('test.observable.throws', {
  observable: (m) => {
    if (throwOnCall) {
      throw new Error('Observable callback error');
    }
    m.record(42);
  },
});

// Also create a normal observable to ensure it still works
let normalValue = 100;
metrics.create('test.observable.normal', {
  observable: (m) => { m.record(normalValue); },
});

const consumer = metrics.createConsumer({
  'test.observable.throws': { aggregation: 'lastValue' },
  'test.observable.normal': { aggregation: 'lastValue' },
});

// Set up uncaughtException handler to catch the error
// (like diagnostics_channel behavior)
process.on('uncaughtException', common.mustCall((err) => {
  assert.strictEqual(err.message, 'Observable callback error');
}, 1));

// First collect - the throwing observable should trigger uncaughtException
// but the normal observable should still work
const result1 = consumer.collect();

// Should have one data point from the normal observable
// (the throwing one didn't produce a value)
assert.strictEqual(result1.length, 1);
assert.strictEqual(result1[0].descriptor.name, 'test.observable.normal');
assert.strictEqual(result1[0].dataPoints[0].value, 100);

// Now stop throwing
throwOnCall = false;
normalValue = 200;

// Give nextTick a chance to fire the uncaughtException
setImmediate(common.mustCall(() => {
  // Second collect - both should work now
  const result2 = consumer.collect();
  assert.strictEqual(result2.length, 2);

  const throwsMetric = result2.find((s) => s.descriptor.name === 'test.observable.throws');
  const normalMetric = result2.find((s) => s.descriptor.name === 'test.observable.normal');

  assert.strictEqual(throwsMetric.dataPoints[0].value, 42);
  assert.strictEqual(normalMetric.dataPoints[0].value, 200);

  consumer.close();
}));
