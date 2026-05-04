// Flags: --expose-internals
'use strict';
require('../common');
const assert = require('node:assert');
const { create, createConsumer, ReservoirSampler, BoundarySampler } = require('internal/perf/metrics');

// This test ensures exemplars work correctly with different aggregation types
// and temporality modes

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

// Test: Exemplars with histogram aggregation
{
  const metric = create('test.histogram.exemplar', { unit: 'ms' });

  const boundaries = [10, 50, 100];
  const sampler = new BoundarySampler(boundaries, extractTraceContext);

  const consumer = createConsumer({
    metrics: {
      'test.histogram.exemplar': {
        aggregation: 'histogram',
        boundaries,
        exemplar: sampler,
      },
    },
  });

  metric.record(5, { traceId: 'trace1', spanId: 'span1' });
  metric.record(30, { traceId: 'trace2', spanId: 'span2' });
  metric.record(75, { traceId: 'trace3', spanId: 'span3' });

  const snapshot = consumer.collect();
  const dataPoints = snapshot[0].dataPoints;

  assert.ok(dataPoints[0].exemplars);
  assert.strictEqual(dataPoints[0].exemplars.length, 3);

  consumer.close();
  metric.close();
}

// Test: Exemplars with summary aggregation
{
  const metric = create('test.summary.exemplar', { unit: 'ms' });

  const sampler = new ReservoirSampler(3, extractTraceContext);

  const consumer = createConsumer({
    metrics: {
      'test.summary.exemplar': {
        aggregation: 'summary',
        exemplar: sampler,
      },
    },
  });

  metric.record(10, { traceId: 'trace1', spanId: 'span1' });
  metric.record(20, { traceId: 'trace2', spanId: 'span2' });
  metric.record(30, { traceId: 'trace3', spanId: 'span3' });

  const snapshot = consumer.collect();
  const dataPoints = snapshot[0].dataPoints;

  assert.ok(dataPoints[0].exemplars);
  assert.strictEqual(dataPoints[0].exemplars.length, 3);

  consumer.close();
  metric.close();
}

// Test: Exemplars with delta temporality
{
  const metric = create('test.delta.exemplar', { unit: 'ms' });

  const sampler = new ReservoirSampler(2, extractTraceContext);

  const consumer = createConsumer({
    defaultTemporality: 'delta',
    metrics: {
      'test.delta.exemplar': {
        aggregation: 'sum',
        exemplar: sampler,
      },
    },
  });

  // First collection period
  metric.record(100, { traceId: 'trace1', spanId: 'span1' });
  metric.record(200, { traceId: 'trace2', spanId: 'span2' });

  let snapshot = consumer.collect();
  assert.strictEqual(snapshot[0].dataPoints[0].sum, 300);
  assert.strictEqual(snapshot[0].dataPoints[0].exemplars.length, 2);

  // Second collection period - should be reset
  metric.record(300, { traceId: 'trace3', spanId: 'span3' });

  snapshot = consumer.collect();
  assert.strictEqual(snapshot[0].dataPoints[0].sum, 300);
  assert.strictEqual(snapshot[0].dataPoints[0].exemplars.length, 1);
  assert.strictEqual(snapshot[0].dataPoints[0].exemplars[0].value, 300);

  consumer.close();
  metric.close();
}

// Test: Exemplars with cumulative temporality
{
  const metric = create('test.cumulative.exemplar', { unit: 'ms' });

  const sampler = new ReservoirSampler(3, extractTraceContext);

  const consumer = createConsumer({
    defaultTemporality: 'cumulative',
    metrics: {
      'test.cumulative.exemplar': {
        aggregation: 'sum',
        exemplar: sampler,
      },
    },
  });

  // First collection
  metric.record(100, { traceId: 'trace1', spanId: 'span1' });
  metric.record(200, { traceId: 'trace2', spanId: 'span2' });

  let snapshot = consumer.collect();
  assert.strictEqual(snapshot[0].dataPoints[0].sum, 300);
  assert.strictEqual(snapshot[0].dataPoints[0].exemplars.length, 2);

  // Second collection - exemplars should accumulate
  metric.record(300, { traceId: 'trace3', spanId: 'span3' });

  snapshot = consumer.collect();
  assert.strictEqual(snapshot[0].dataPoints[0].sum, 600);
  assert.strictEqual(snapshot[0].dataPoints[0].exemplars.length, 3);

  consumer.close();
  metric.close();
}

// Test: Exemplars with groupByAttributes
{
  const metric = create('test.grouped.exemplar', { unit: 'ms' });

  const sampler = new ReservoirSampler(3, extractTraceContext);

  const consumer = createConsumer({
    groupByAttributes: true,
    metrics: {
      'test.grouped.exemplar': {
        aggregation: 'sum',
        groupBy: ['endpoint'],
        exemplar: sampler,
      },
    },
  });

  metric.record(100, { endpoint: '/api/users', traceId: 'trace1', spanId: 'span1' });
  metric.record(200, { endpoint: '/api/posts', traceId: 'trace2', spanId: 'span2' });
  metric.record(150, { endpoint: '/api/users', traceId: 'trace3', spanId: 'span3' });

  const snapshot = consumer.collect();
  const dataPoints = snapshot[0].dataPoints;

  // Should have 2 data points (one per endpoint)
  assert.strictEqual(dataPoints.length, 2);

  // Both should share the same exemplars (sampler is shared)
  assert.ok(dataPoints[0].exemplars);
  assert.ok(dataPoints[1].exemplars);
  assert.strictEqual(dataPoints[0].exemplars.length, dataPoints[1].exemplars.length);

  consumer.close();
  metric.close();
}

// Test: No exemplars field when empty
{
  const metric = create('test.no.exemplar', { unit: 'ms' });

  const consumer = createConsumer({
    metrics: {
      'test.no.exemplar': {
        aggregation: 'sum',
        // No exemplar sampler configured
      },
    },
  });

  metric.record(100);
  metric.record(200);

  const snapshot = consumer.collect();
  const dataPoints = snapshot[0].dataPoints;

  // Should not have exemplars field
  assert.strictEqual(dataPoints[0].exemplars, undefined);

  consumer.close();
  metric.close();
}

// Test: Empty exemplars don't add field
{
  const metric = create('test.empty.exemplar', { unit: 'ms' });

  const sampler = new ReservoirSampler(3, extractTraceContext);

  const consumer = createConsumer({
    metrics: {
      'test.empty.exemplar': {
        aggregation: 'sum',
        exemplar: sampler,
      },
    },
  });

  // Record without trace context
  metric.record(100);
  metric.record(200);

  const snapshot = consumer.collect();
  const dataPoints = snapshot[0].dataPoints;

  // Should not have exemplars field when no exemplars collected
  assert.strictEqual(dataPoints[0].exemplars, undefined);

  consumer.close();
  metric.close();
}

// Test: Invalid sampler throws
{
  assert.throws(() => {
    createConsumer({
      metrics: {
        'test.invalid': {
          exemplar: { sample: 'not a function' },
        },
      },
    });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.throws(() => {
    createConsumer({
      metrics: {
        'test.invalid': {
          exemplar: {
            sample: () => {},
            // Missing getExemplars and reset
          },
        },
      },
    });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
  });
}
