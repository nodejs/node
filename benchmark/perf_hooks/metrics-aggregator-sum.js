'use strict';

const assert = require('assert');
const common = require('../common.js');

const { metrics } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e7, 1e8],
  monotonic: ['yes', 'no'],
  valueType: ['number', 'bigint'],
});

function main({ n, monotonic, valueType }) {
  const metric = metrics.create('bench.aggregator.sum', { unit: 'count' });

  const consumer = metrics.createConsumer({
    'bench.aggregator.sum': {
      aggregation: 'sum',
      monotonic: monotonic === 'yes',
    },
  });

  const useBigInt = valueType === 'bigint';
  const increment = useBigInt ? 1n : 1;
  const decrement = useBigInt ? -1n : -1;

  bench.start();
  if (monotonic === 'yes') {
    // Monotonic - always positive values
    for (let i = 0; i < n; i++) {
      metric.record(increment);
    }
  } else {
    // Non-monotonic - mix of positive and negative
    for (let i = 0; i < n; i++) {
      metric.record(i % 2 === 0 ? increment : decrement);
    }
  }
  bench.end(n);

  // Cleanup and avoid deadcode elimination
  assert.ok(consumer.collect());
  consumer.close();
}
