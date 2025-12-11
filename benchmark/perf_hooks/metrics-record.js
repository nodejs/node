'use strict';

const assert = require('assert');
const common = require('../common.js');

const { metrics } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e7, 1e8],
  consumerCount: [1, 5, 10],
  aggregation: ['sum', 'lastValue', 'histogram'],
});

function main({ n, consumerCount, aggregation }) {
  const metric = metrics.create('bench.record', { unit: 'count' });

  // Create consumers with the specified aggregation
  const consumers = [];
  for (let i = 0; i < consumerCount; i++) {
    consumers.push(metrics.createConsumer({
      'bench.record': { aggregation },
    }));
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    metric.record(i);
  }
  bench.end(n);

  // Cleanup and avoid deadcode elimination
  for (const consumer of consumers) {
    assert.ok(consumer.collect());
    consumer.close();
  }
}
