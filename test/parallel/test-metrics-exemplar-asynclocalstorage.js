// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const { create, createConsumer, ReservoirSampler } = require('internal/perf/metrics');

// This test demonstrates how to use AsyncLocalStorage to automatically
// propagate trace context to exemplar samples

const traceStorage = new AsyncLocalStorage();

// Extract function that reads from AsyncLocalStorage
function extractTraceContext(attributes) {
  const store = traceStorage.getStore();
  if (!store || !store.traceId || !store.spanId) {
    return null;
  }
  return {
    traceId: store.traceId,
    spanId: store.spanId,
    filteredAttributes: attributes,
  };
}

// Test: Trace context propagates through AsyncLocalStorage
{
  const metric = create('test.als.exemplar', { unit: 'ms' });

  const sampler = new ReservoirSampler(5, extractTraceContext);

  const consumer = createConsumer({
    metrics: {
      'test.als.exemplar': {
        aggregation: 'sum',
        exemplar: sampler,
      },
    },
  });

  // Simulate a request with trace context
  traceStorage.run({ traceId: 'trace-123', spanId: 'span-456' }, () => {
    metric.record(100, { endpoint: '/api/users' });
    metric.record(200, { endpoint: '/api/posts' });
  });

  // Another request with different trace context
  traceStorage.run({ traceId: 'trace-789', spanId: 'span-abc' }, () => {
    metric.record(150, { endpoint: '/api/users' });
  });

  const snapshot = consumer.collect();
  const dataPoints = snapshot[0].dataPoints;

  assert.strictEqual(dataPoints[0].sum, 450);
  assert.ok(dataPoints[0].exemplars);
  assert.strictEqual(dataPoints[0].exemplars.length, 3);

  // Check trace contexts
  const traceIds = dataPoints[0].exemplars.map((e) => e.traceId);
  assert.ok(traceIds.includes('trace-123'));
  assert.ok(traceIds.includes('trace-789'));

  // Check filtered attributes
  assert.strictEqual(dataPoints[0].exemplars[0].filteredAttributes.endpoint, '/api/users');

  consumer.close();
  metric.close();
}

// Test: No trace context results in no exemplars
{
  const metric = create('test.als.no.context', { unit: 'ms' });

  const sampler = new ReservoirSampler(3, extractTraceContext);

  const consumer = createConsumer({
    metrics: {
      'test.als.no.context': {
        aggregation: 'sum',
        exemplar: sampler,
      },
    },
  });

  // Record without AsyncLocalStorage context
  metric.record(100);
  metric.record(200);

  const snapshot = consumer.collect();
  const dataPoints = snapshot[0].dataPoints;

  assert.strictEqual(dataPoints[0].sum, 300);
  // No exemplars should be collected
  assert.strictEqual(dataPoints[0].exemplars, undefined);

  consumer.close();
  metric.close();
}

// Test: Nested contexts
{
  const metric = create('test.als.nested', { unit: 'ms' });

  const sampler = new ReservoirSampler(5, extractTraceContext);

  const consumer = createConsumer({
    metrics: {
      'test.als.nested': {
        aggregation: 'sum',
        exemplar: sampler,
      },
    },
  });

  traceStorage.run({ traceId: 'trace-parent', spanId: 'span-parent' }, () => {
    metric.record(100);

    // Nested span
    traceStorage.run({ traceId: 'trace-parent', spanId: 'span-child' }, () => {
      metric.record(200);
    });

    metric.record(150);
  });

  const snapshot = consumer.collect();
  const dataPoints = snapshot[0].dataPoints;

  assert.strictEqual(dataPoints[0].sum, 450);
  assert.ok(dataPoints[0].exemplars);
  assert.strictEqual(dataPoints[0].exemplars.length, 3);

  // Should have both parent and child span IDs
  const spanIds = dataPoints[0].exemplars.map((e) => e.spanId);
  assert.ok(spanIds.includes('span-parent'));
  assert.ok(spanIds.includes('span-child'));

  consumer.close();
  metric.close();
}

// Test: Timer with AsyncLocalStorage
{
  const metric = create('test.als.timer', { unit: 'ms' });

  const sampler = new ReservoirSampler(3, extractTraceContext);

  const consumer = createConsumer({
    metrics: {
      'test.als.timer': {
        aggregation: 'sum',
        exemplar: sampler,
      },
    },
  });

  traceStorage.run({ traceId: 'trace-timer', spanId: 'span-timer' }, () => {
    const timer = metric.startTimer();
    // Simulate some work
    timer.stop();
  });

  const snapshot = consumer.collect();

  if (snapshot.length > 0) {
    const dataPoints = snapshot[0].dataPoints;
    assert.ok(dataPoints[0].exemplars);
    assert.strictEqual(dataPoints[0].exemplars.length, 1);
    assert.strictEqual(dataPoints[0].exemplars[0].traceId, 'trace-timer');
    assert.strictEqual(dataPoints[0].exemplars[0].spanId, 'span-timer');
  }

  consumer.close();
  metric.close();
}

// Test: Concurrent requests with different contexts
{
  const metric = create('test.als.concurrent', { unit: 'ms' });

  const sampler = new ReservoirSampler(10, extractTraceContext);

  const consumer = createConsumer({
    metrics: {
      'test.als.concurrent': {
        aggregation: 'sum',
        exemplar: sampler,
      },
    },
  });

  const promises = [];

  // Simulate 5 concurrent requests
  for (let i = 0; i < 5; i++) {
    const promise = new Promise((resolve) => {
      traceStorage.run({ traceId: `trace-${i}`, spanId: `span-${i}` }, () => {
        setImmediate(() => {
          metric.record(100 + i * 10);
          resolve();
        });
      });
    });
    promises.push(promise);
  }

  Promise.all(promises).then(() => {
    const snapshot = consumer.collect();
    const dataPoints = snapshot[0].dataPoints;

    assert.strictEqual(dataPoints[0].sum, 600);
    assert.ok(dataPoints[0].exemplars);
    assert.strictEqual(dataPoints[0].exemplars.length, 5);

    // Check all trace IDs are unique
    const traceIds = dataPoints[0].exemplars.map((e) => e.traceId);
    const uniqueTraceIds = new Set(traceIds);
    assert.strictEqual(uniqueTraceIds.size, 5);

    consumer.close();
    metric.close();
  });
}
