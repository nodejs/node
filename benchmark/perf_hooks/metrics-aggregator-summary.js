'use strict';

const assert = require('assert');
const common = require('../common.js');

const { metrics } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e6, 1e7],
  quantileCount: [3, 5, 10],
});

function main({ n, quantileCount }) {
  const metric = metrics.create('bench.aggregator.summary', { unit: 'ms' });

  // Generate quantiles
  const quantiles = [];
  for (let i = 1; i <= quantileCount; i++) {
    quantiles.push(i / (quantileCount + 1));
  }

  const consumer = metrics.createConsumer({
    'bench.aggregator.summary': {
      aggregation: 'summary',
      quantiles,
    },
  });

  bench.start();
  for (let i = 0; i < n; i++) {
    // Record values that simulate realistic latency distribution (min 1)
    metric.record(Math.random() * 999 + 1);
  }
  bench.end(n);

  // Cleanup and avoid deadcode elimination
  assert.ok(consumer.collect());
  consumer.close();
}
