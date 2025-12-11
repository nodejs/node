'use strict';

const assert = require('assert');
const common = require('../common.js');

const { metrics } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e5, 1e6],
  metricsCount: [10, 100],
  attrCardinality: [1, 10, 100],
  aggregation: ['sum', 'histogram'],
});

function main({ n, metricsCount, attrCardinality, aggregation }) {
  // Create multiple metrics
  const metricsList = [];
  const subscriptions = {};

  for (let i = 0; i < metricsCount; i++) {
    const name = `bench.collect.${i}`;
    metricsList.push(metrics.create(name, { unit: 'count' }));
    subscriptions[name] = { aggregation };
  }

  const consumer = metrics.createConsumer(subscriptions);

  // Pre-populate data with attribute cardinality
  for (const metric of metricsList) {
    for (let c = 0; c < attrCardinality; c++) {
      metric.record(1, { variant: `v${c}` });
    }
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    consumer.collect();
  }
  bench.end(n);

  // Cleanup and avoid deadcode elimination
  assert.ok(consumer);
  consumer.close();
}
