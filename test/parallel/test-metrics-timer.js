'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test Timer duration measurement
const m = metrics.create('test.timer', { unit: 'ms' });

const consumer = metrics.createConsumer({
  'test.timer': { aggregation: 'sum' },
});

// Start timer
const timer = m.startTimer();
assert.ok(timer);

// Do some work
for (let i = 0; i < 100000; i++) {
  // Simulate work
}

// Stop timer and get duration
const duration = timer.stop();
assert.ok(typeof duration === 'number');
assert.ok(duration > 0);
assert.ok(duration < 10000); // Should be less than 10 seconds

// Check consumer received the value
const result = consumer.collect();
assert.strictEqual(result[0].dataPoints[0].count, 1);
assert.ok(result[0].dataPoints[0].sum > 0);

consumer.close();

// Test timer with attributes (groupByAttributes: true enables attribute tracking)
const m2 = metrics.create('test.timer.attrs', { unit: 'ms' });

const consumer2 = metrics.createConsumer({
  'groupByAttributes': true,
  'test.timer.attrs': { aggregation: 'sum' },
});

const timer2 = m2.startTimer({ operation: 'test' });
timer2.stop();

const result2 = consumer2.collect();
assert.deepStrictEqual(result2[0].dataPoints[0].attributes, { operation: 'test' });

consumer2.close();

// Test double-stop throws
const m3 = metrics.create('test.timer.double');
const timer3 = m3.startTimer();
timer3.stop();
assert.throws(() => timer3.stop(), {
  code: 'ERR_INVALID_STATE',
});
