'use strict';

const assert = require('assert');
const common = require('../common.js');

const { metrics } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e7, 1e8],
  attrCount: [0, 1, 5, 10],
  attrCardinality: [1, 10, 100],
});

function main({ n, attrCount, attrCardinality }) {
  const metric = metrics.create('bench.record.attrs', { unit: 'count' });

  const consumer = metrics.createConsumer({
    'bench.record.attrs': { aggregation: 'sum' },
  });

  // Pre-generate attribute objects with specified cardinality
  const attributeSets = [];
  for (let c = 0; c < attrCardinality; c++) {
    const attrs = {};
    for (let a = 0; a < attrCount; a++) {
      attrs[`attr${a}`] = `value${c}`;
    }
    attributeSets.push(attrs);
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    const attrs = attrCount > 0 ? attributeSets[i % attrCardinality] : undefined;
    metric.record(i, attrs);
  }
  bench.end(n);

  // Cleanup and avoid deadcode elimination
  assert.ok(consumer.collect());
  consumer.close();
}
