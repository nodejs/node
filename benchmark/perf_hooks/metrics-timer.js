'use strict';

const assert = require('assert');
const common = require('../common.js');

const { metrics } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e6, 1e7],
  method: ['startStop', 'using'],
  consumerCount: [1, 5],
});

function main({ n, method, consumerCount }) {
  const metric = metrics.create('bench.timer', { unit: 'ms' });

  // Create consumers
  const consumers = [];
  for (let i = 0; i < consumerCount; i++) {
    consumers.push(metrics.createConsumer({
      'bench.timer': { aggregation: 'sum' },
    }));
  }

  bench.start();
  if (method === 'startStop') {
    // Manual start/stop
    for (let i = 0; i < n; i++) {
      const timer = metric.startTimer();
      timer.stop();
    }
  } else {
    // Using Symbol.dispose
    for (let i = 0; i < n; i++) {
      // eslint-disable-next-line no-unused-vars
      using timer = metric.startTimer();
    }
  }
  bench.end(n);

  // Cleanup and avoid deadcode elimination
  for (const consumer of consumers) {
    assert.ok(consumer.collect());
    consumer.close();
  }
}
