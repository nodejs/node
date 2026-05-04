'use strict';

const assert = require('assert');
const common = require('../common.js');
const dc = require('diagnostics_channel');

const { metrics } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e7, 1e8],
  hasSubscribers: ['yes', 'no'],
});

function main({ n, hasSubscribers }) {
  const metric = metrics.create('bench.dc', { unit: 'count' });

  // Create DC consumer
  const dcConsumer = metrics.createDiagnosticsChannelConsumer();

  // Optionally subscribe to the channel
  let received = 0;
  if (hasSubscribers === 'yes') {
    dc.subscribe('metrics:bench.dc', () => {
      received++;
    });
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    metric.record(i);
  }
  bench.end(n);

  // Cleanup and avoid deadcode elimination
  assert.ok(dcConsumer);
  if (hasSubscribers === 'yes') {
    assert.strictEqual(received, n);
  }
  dcConsumer.close();
}
