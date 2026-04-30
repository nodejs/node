'use strict';

const assert = require('assert');
const common = require('../common.js');

const { metrics } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e7, 1e8],
  boundaryCount: [5, 10, 20, 50],
});

function main({ n, boundaryCount }) {
  const metric = metrics.create('bench.aggregator.histogram', { unit: 'ms' });

  // Generate exponential boundaries like OTel defaults
  const boundaries = [];
  for (let i = 0; i < boundaryCount; i++) {
    boundaries.push(Math.pow(2, i));
  }

  const consumer = metrics.createConsumer({
    'bench.aggregator.histogram': {
      aggregation: 'histogram',
      boundaries,
    },
  });

  // Generate values that spread across buckets
  const maxBoundary = boundaries[boundaries.length - 1];

  bench.start();
  for (let i = 0; i < n; i++) {
    // Distribute values across the range
    const value = (i % (maxBoundary + 1));
    metric.record(value);
  }
  bench.end(n);

  // Cleanup and avoid deadcode elimination
  assert.ok(consumer.collect());
  consumer.close();
}
