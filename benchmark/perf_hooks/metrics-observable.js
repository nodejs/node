'use strict';

const assert = require('assert');
const common = require('../common.js');

const { metrics } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e5, 1e6],
  observableCount: [1, 10, 100],
});

function main({ n, observableCount }) {
  // Create observable metrics
  const subscriptions = {};
  let counter = 0;

  for (let i = 0; i < observableCount; i++) {
    const name = `bench.observable.${i}`;
    metrics.create(name, {
      unit: 'count',
      observable: (m) => { m.record(counter++); },
    });
    subscriptions[name] = { aggregation: 'lastValue' };
  }

  const consumer = metrics.createConsumer(subscriptions);

  bench.start();
  for (let i = 0; i < n; i++) {
    consumer.collect();
  }
  bench.end(n);

  // Cleanup and avoid deadcode elimination
  assert.ok(counter > 0);
  consumer.close();
}
