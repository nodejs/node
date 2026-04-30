'use strict';

require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test Timer Symbol.dispose with `using` syntax
const m = metrics.create('test.timer.dispose', { unit: 'ms' });

const consumer = metrics.createConsumer({
  'test.timer.dispose': { aggregation: 'sum' },
});

// Test using syntax (Symbol.dispose)
{
  // eslint-disable-next-line no-unused-vars
  using _timer = m.startTimer();
  // Do some work
  for (let i = 0; i < 10000; i++) {
    // Simulate work
  }
  // Timer should auto-stop when leaving block
}

// Verify value was recorded
const result = consumer.collect();
assert.strictEqual(result[0].dataPoints[0].count, 1);
assert.ok(result[0].dataPoints[0].sum > 0);

consumer.close();

// Test that Symbol.dispose is defined
const timer = m.startTimer();
assert.strictEqual(typeof timer[Symbol.dispose], 'function');
timer.stop();
