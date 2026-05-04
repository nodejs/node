'use strict';

const common = require('../common');

const assert = require('assert');
const { metrics } = require('perf_hooks');

// Test autoCollect basic functionality
{
  const m = metrics.create('test.autocollect.basic');
  const consumer = metrics.createConsumer({
    'test.autocollect.basic': { aggregation: 'sum', temporality: 'delta' },
  });

  let callCount = 0;
  const stop = consumer.autoCollect(20, common.mustCallAtLeast((result) => {
    callCount++;
    if (callCount === 1) {
      // First result should have our recorded value
      assert.strictEqual(result.length, 1);
      assert.strictEqual(result[0].dataPoints[0].sum, 42);
    } else if (callCount === 2) {
      // Second result should be empty (no new values, delta reset)
      assert.strictEqual(result.length, 0);
      stop();
      consumer.close();
    }
  }, 2));

  // Record a value before first collection
  m.record(42);
}

// Test autoCollect throws if already active
{
  const consumer = metrics.createConsumer();
  const stop = consumer.autoCollect(1000, () => {});

  assert.throws(() => {
    consumer.autoCollect(1000, () => {});
  }, {
    code: 'ERR_INVALID_STATE',
    message: /autoCollect is already active/,
  });

  stop();
  consumer.close();
}

// Test autoCollect throws if consumer is closed
{
  const consumer = metrics.createConsumer();
  consumer.close();

  assert.throws(() => {
    consumer.autoCollect(1000, () => {});
  }, {
    code: 'ERR_INVALID_STATE',
    message: /Consumer is closed/,
  });
}

// Test stop function can be called multiple times safely
{
  const consumer = metrics.createConsumer();
  const stop = consumer.autoCollect(1000, () => {});

  stop();
  stop(); // Should not throw
  consumer.close();
}

// Test close() stops autoCollect
{
  const consumer = metrics.createConsumer();
  let callCount = 0;
  consumer.autoCollect(10, () => {
    callCount++;
  });

  // Close immediately - should stop the timer
  consumer.close();

  // Wait a bit to ensure no more calls happen
  setTimeout(common.mustCall(() => {
    assert.strictEqual(callCount, 0);
  }), 50);
}

// Test validation
{
  const consumer = metrics.createConsumer();

  assert.throws(() => {
    consumer.autoCollect('not a number', () => {});
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    consumer.autoCollect(100, 'not a function');
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    // Interval must be >= 1
    consumer.autoCollect(0, () => {});
  }, {
    code: 'ERR_OUT_OF_RANGE',
  });

  consumer.close();
}
