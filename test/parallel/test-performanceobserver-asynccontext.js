'use strict';

const common = require('../common');
const assert = require('assert');
const {
  performance,
  PerformanceObserver,
} = require('perf_hooks');
const {
  executionAsyncId,
  triggerAsyncId,
  executionAsyncResource,
} = require('async_hooks');

// Test Non-Buffered PerformanceObserver retains async context
{
  const observer =
    new PerformanceObserver(common.mustCall(callback));

  const initialAsyncId = executionAsyncId();
  let asyncIdInTimeout;
  let asyncResourceInTimeout;

  function callback(list) {
    assert.strictEqual(triggerAsyncId(), initialAsyncId);
    assert.strictEqual(executionAsyncId(), asyncIdInTimeout);
    assert.strictEqual(executionAsyncResource(), asyncResourceInTimeout);
    observer.disconnect();
  }
  observer.observe({ entryTypes: ['mark'] });

  setTimeout(() => {
    asyncIdInTimeout = executionAsyncId();
    asyncResourceInTimeout = executionAsyncResource();
    performance.mark('test1');
  }, 0);
}
