// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('node:assert');
const { create, createConsumer, ReservoirSampler } = require('internal/perf/metrics');

// This test ensures that ReservoirSampler correctly samples exemplars
// using reservoir sampling algorithm

// Simple extract function that returns trace context
function extractTraceContext(attributes) {
  if (!attributes.traceId || !attributes.spanId) {
    return null;
  }
  return {
    traceId: attributes.traceId,
    spanId: attributes.spanId,
    filteredAttributes: {},
  };
}

// Test: Reservoir fills up to max capacity
{
  const sampler = new ReservoirSampler(3, extractTraceContext);

  sampler.sample(10, 1000, { traceId: 'trace1', spanId: 'span1' });
  sampler.sample(20, 2000, { traceId: 'trace2', spanId: 'span2' });
  sampler.sample(30, 3000, { traceId: 'trace3', spanId: 'span3' });

  const exemplars = sampler.getExemplars();
  assert.strictEqual(exemplars.length, 3);
  assert.strictEqual(exemplars[0].value, 10);
  assert.strictEqual(exemplars[1].value, 20);
  assert.strictEqual(exemplars[2].value, 30);
}

// Test: Reservoir replacement with probability
{
  const sampler = new ReservoirSampler(2, extractTraceContext);

  // Fill reservoir
  sampler.sample(10, 1000, { traceId: 'trace1', spanId: 'span1' });
  sampler.sample(20, 2000, { traceId: 'trace2', spanId: 'span2' });

  // Add many more samples - some should replace existing ones
  for (let i = 0; i < 100; i++) {
    sampler.sample(30 + i, 3000 + i, {
      traceId: `trace${i + 3}`,
      spanId: `span${i + 3}`,
    });
  }

  const exemplars = sampler.getExemplars();
  assert.strictEqual(exemplars.length, 2);
  // At least one should have been replaced (probabilistically)
  const hasReplacement = exemplars.some((e) => e.value >= 30);
  assert.strictEqual(hasReplacement, true);
}

// Test: Reset clears reservoir
{
  const sampler = new ReservoirSampler(3, extractTraceContext);

  sampler.sample(10, 1000, { traceId: 'trace1', spanId: 'span1' });
  sampler.sample(20, 2000, { traceId: 'trace2', spanId: 'span2' });

  assert.strictEqual(sampler.getExemplars().length, 2);

  sampler.reset();

  assert.strictEqual(sampler.getExemplars().length, 0);
}

// Test: Skip samples without trace context
{
  const sampler = new ReservoirSampler(3, extractTraceContext);

  sampler.sample(10, 1000, {}); // No trace context
  sampler.sample(20, 2000, { traceId: 'trace1' }); // Missing spanId
  sampler.sample(30, 3000, { spanId: 'span1' }); // Missing traceId

  assert.strictEqual(sampler.getExemplars().length, 0);
}

// Test: Integration with consumer
{
  const metric = create('test.reservoir', { unit: 'ms' });

  const sampler = new ReservoirSampler(3, extractTraceContext);

  const consumer = createConsumer({
    defaultAggregation: 'sum',
    defaultTemporality: 'cumulative',
    metrics: {
      'test.reservoir': {
        exemplar: sampler,
      },
    },
  });

  // Record values with trace context
  metric.record(100, { traceId: 'trace1', spanId: 'span1' });
  metric.record(200, { traceId: 'trace2', spanId: 'span2' });
  metric.record(300, { traceId: 'trace3', spanId: 'span3' });

  const snapshot = consumer.collect();
  assert.strictEqual(snapshot.length, 1);

  const dataPoints = snapshot[0].dataPoints;
  assert.strictEqual(dataPoints.length, 1);
  assert.strictEqual(dataPoints[0].sum, 600);
  assert.strictEqual(dataPoints[0].count, 3);

  // Check exemplars
  assert.ok(dataPoints[0].exemplars);
  assert.strictEqual(dataPoints[0].exemplars.length, 3);
  assert.strictEqual(dataPoints[0].exemplars[0].value, 100);
  assert.strictEqual(dataPoints[0].exemplars[0].traceId, 'trace1');
  assert.strictEqual(dataPoints[0].exemplars[0].spanId, 'span1');

  consumer.close();
  metric.close();
}

// Test: Exemplars reset for delta temporality
{
  const metric = create('test.delta.reservoir', { unit: 'ms' });

  const sampler = new ReservoirSampler(2, extractTraceContext);

  const consumer = createConsumer({
    defaultAggregation: 'sum',
    defaultTemporality: 'delta',
    metrics: {
      'test.delta.reservoir': {
        exemplar: sampler,
      },
    },
  });

  // First collection
  metric.record(100, { traceId: 'trace1', spanId: 'span1' });
  metric.record(200, { traceId: 'trace2', spanId: 'span2' });

  let snapshot = consumer.collect();
  assert.strictEqual(snapshot[0].dataPoints[0].exemplars.length, 2);

  // Second collection - exemplars should be reset
  metric.record(300, { traceId: 'trace3', spanId: 'span3' });

  snapshot = consumer.collect();
  assert.strictEqual(snapshot[0].dataPoints[0].exemplars.length, 1);
  assert.strictEqual(snapshot[0].dataPoints[0].exemplars[0].value, 300);

  consumer.close();
  metric.close();
}
