'use strict';
require('../common');
const assert = require('node:assert');
const { metrics } = require('node:perf_hooks');

// This test ensures that validation is properly enforced throughout the metrics API.

{
  // Test: Empty string metric name throws
  assert.throws(() => {
    metrics.create('', {
      description: 'Invalid empty name',
    });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
  });
}

{
  // Test: Non-string metric name throws
  assert.throws(() => {
    metrics.create(123, {
      description: 'Invalid number name',
    });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    metrics.create(null, {
      description: 'Invalid null name',
    });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    metrics.create(undefined, {
      description: 'Invalid undefined name',
    });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}

{
  // Test: autoCollect() with interval < 1 throws
  const metric = metrics.create('test.autocollect.interval', {
    description: 'Test interval validation',
  });

  const consumer = metrics.createConsumer({
    'test.autocollect.interval': {},
  });

  assert.throws(() => {
    consumer.autoCollect(0, () => {});
  }, {
    code: 'ERR_OUT_OF_RANGE',
  });

  assert.throws(() => {
    consumer.autoCollect(-1, () => {});
  }, {
    code: 'ERR_OUT_OF_RANGE',
  });

  consumer.close();
  metric.close();
}

{
  // Test: autoCollect() with non-function callback throws
  const metric = metrics.create('test.autocollect.callback', {
    description: 'Test callback validation',
  });

  const consumer = metrics.createConsumer({
    'test.autocollect.callback': {},
  });

  assert.throws(() => {
    consumer.autoCollect(1000, 'not a function');
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    consumer.autoCollect(1000, null);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  consumer.close();
  metric.close();
}

{
  // Test: autoCollect() when already active throws
  const metric = metrics.create('test.autocollect.duplicate', {
    description: 'Test duplicate autoCollect',
  });

  const consumer = metrics.createConsumer({
    'test.autocollect.duplicate': {},
  });

  const stop = consumer.autoCollect(1000, () => {});

  assert.throws(() => {
    consumer.autoCollect(1000, () => {});
  }, {
    code: 'ERR_INVALID_STATE',
  });

  stop(); // Call the stop function
  consumer.close();
  metric.close();
}

{
  // Test: autoCollect() on closed consumer throws
  const metric = metrics.create('test.autocollect.closed', {
    description: 'Test autoCollect on closed consumer',
  });

  const consumer = metrics.createConsumer({
    'test.autocollect.closed': {},
  });

  consumer.close();

  assert.throws(() => {
    consumer.autoCollect(1000, () => {});
  }, {
    code: 'ERR_INVALID_STATE',
  });

  metric.close();
}

{
  // Test: record() with non-number/bigint throws
  const metric = metrics.create('test.record.validation', {
    description: 'Test record validation',
  });

  assert.throws(() => {
    metric.record('not a number');
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    metric.record(null);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    metric.record(undefined);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    metric.record([10]);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  metric.close();
}

{
  // Test: record() with non-object attributes throws
  const metric = metrics.create('test.attributes.validation', {
    description: 'Test attributes validation',
  });

  assert.throws(() => {
    metric.record(10, 'not an object');
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    metric.record(10, 123);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    metric.record(10, []);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  metric.close();
}

{
  // Test: Timer constructor with non-object attributes throws
  const metric = metrics.create('test.timer.attributes', {
    description: 'Test timer attributes validation',
  });

  assert.throws(() => {
    metric.startTimer('not an object');
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    metric.startTimer(123);
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  metric.close();
}

{
  // Test: Observable must be a function
  assert.throws(() => {
    metrics.create('test.bad.observable', {
      observable: 'not a function',
    });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    metrics.create('test.bad.observable2', {
      observable: 123,
    });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}
