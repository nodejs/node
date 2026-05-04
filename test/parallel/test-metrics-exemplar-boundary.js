// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('node:assert');
const { create, createConsumer, BoundarySampler } = require('internal/perf/metrics');

// This test ensures that BoundarySampler correctly maintains one exemplar
// per histogram bucket

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

// Test: One exemplar per bucket
{
  const boundaries = [10, 50, 100];
  const sampler = new BoundarySampler(boundaries, extractTraceContext);

  // Add samples in different buckets
  sampler.sample(5, 1000, { traceId: 'trace1', spanId: 'span1' });    // Bucket 0 (≤10)
  sampler.sample(30, 2000, { traceId: 'trace2', spanId: 'span2' });   // Bucket 1 (≤50)
  sampler.sample(75, 3000, { traceId: 'trace3', spanId: 'span3' });   // Bucket 2 (≤100)
  sampler.sample(150, 4000, { traceId: 'trace4', spanId: 'span4' });  // Bucket 3 (>100)

  const exemplars = sampler.getExemplars();
  assert.strictEqual(exemplars.length, 4);

  // Check values are from different buckets
  const values = exemplars.map((e) => e.value).sort((a, b) => a - b);
  assert.deepStrictEqual(values, [5, 30, 75, 150]);
}

// Test: Replacement in same bucket (10% probability)
{
  const boundaries = [10];
  const sampler = new BoundarySampler(boundaries, extractTraceContext);

  // Add many samples to same bucket
  for (let i = 0; i < 100; i++) {
    sampler.sample(5, 1000 + i, {
      traceId: `trace${i}`,
      spanId: `span${i}`,
    });
  }

  const exemplars = sampler.getExemplars();
  // Should still have only 1-2 exemplars (one per bucket, possibly overflow)
  assert.ok(exemplars.length <= 2);
}

// Test: Overflow bucket
{
  const boundaries = [10, 50, 100];
  const sampler = new BoundarySampler(boundaries, extractTraceContext);

  sampler.sample(200, 1000, { traceId: 'trace1', spanId: 'span1' });
  sampler.sample(300, 2000, { traceId: 'trace2', spanId: 'span2' });

  const exemplars = sampler.getExemplars();
  assert.strictEqual(exemplars.length, 1);
  // Should have exemplar from overflow bucket
  assert.ok(exemplars[0].value >= 200);
}

// Test: Boundary edge cases
{
  const boundaries = [10, 50, 100];
  const sampler = new BoundarySampler(boundaries, extractTraceContext);

  sampler.sample(10, 1000, { traceId: 'trace1', spanId: 'span1' });   // Exactly on boundary
  sampler.sample(50, 2000, { traceId: 'trace2', spanId: 'span2' });   // Exactly on boundary
  sampler.sample(100, 3000, { traceId: 'trace3', spanId: 'span3' });  // Exactly on boundary

  const exemplars = sampler.getExemplars();
  assert.strictEqual(exemplars.length, 3);
}

// Test: Reset clears exemplars
{
  const boundaries = [10, 50];
  const sampler = new BoundarySampler(boundaries, extractTraceContext);

  sampler.sample(5, 1000, { traceId: 'trace1', spanId: 'span1' });
  sampler.sample(30, 2000, { traceId: 'trace2', spanId: 'span2' });

  assert.strictEqual(sampler.getExemplars().length, 2);

  sampler.reset();

  assert.strictEqual(sampler.getExemplars().length, 0);
}

// Test: Skip samples without trace context
{
  const boundaries = [10, 50];
  const sampler = new BoundarySampler(boundaries, extractTraceContext);

  sampler.sample(5, 1000, {}); // No trace context
  sampler.sample(30, 2000, { traceId: 'trace1' }); // Missing spanId

  assert.strictEqual(sampler.getExemplars().length, 0);
}

// Test: Integration with histogram consumer
{
  const metric = create('test.boundary', { unit: 'ms' });

  const boundaries = [10, 50, 100];
  const sampler = new BoundarySampler(boundaries, extractTraceContext);

  const consumer = createConsumer({
    defaultAggregation: 'histogram',
    defaultTemporality: 'cumulative',
    metrics: {
      'test.boundary': {
        boundaries,
        exemplar: sampler,
      },
    },
  });

  // Record values in different buckets
  metric.record(5, { traceId: 'trace1', spanId: 'span1' });
  metric.record(30, { traceId: 'trace2', spanId: 'span2' });
  metric.record(75, { traceId: 'trace3', spanId: 'span3' });
  metric.record(150, { traceId: 'trace4', spanId: 'span4' });

  const snapshot = consumer.collect();
  assert.strictEqual(snapshot.length, 1);

  const dataPoints = snapshot[0].dataPoints;
  assert.strictEqual(dataPoints.length, 1);

  // Check histogram data
  assert.strictEqual(dataPoints[0].count, 4);
  assert.strictEqual(dataPoints[0].sum, 260);

  // Check exemplars
  assert.ok(dataPoints[0].exemplars);
  assert.strictEqual(dataPoints[0].exemplars.length, 4);

  consumer.close();
  metric.close();
}

// Test: Large boundary array uses binary search
{
  // Create boundaries array larger than 8 to trigger binary search path
  const boundaries = [10, 20, 30, 40, 50, 60, 70, 80, 90, 100];
  const sampler = new BoundarySampler(boundaries, extractTraceContext);

  // Add samples in different buckets
  sampler.sample(5, 1000, { traceId: 'trace1', spanId: 'span1' });
  sampler.sample(45, 2000, { traceId: 'trace2', spanId: 'span2' });
  sampler.sample(95, 3000, { traceId: 'trace3', spanId: 'span3' });
  sampler.sample(105, 4000, { traceId: 'trace4', spanId: 'span4' });

  const exemplars = sampler.getExemplars();
  assert.strictEqual(exemplars.length, 4);
}
