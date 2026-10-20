'use strict';

const common = require('../common.js');
const assert = require('node:assert');
const fs = require('node:fs/promises');

const {
  performance,
} = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e6],
  events: [1, 1000, 10000],
  api: ['number', 'bigint'],
});

async function runEvents(events) {
  for (let i = 0; i < events; ++i) {
    assert.ok(await fs.statfs(__filename));
  }
}

async function main({ n, events, api }) {
  await runEvents(events);
  if (api === 'bigint') {
    bench.start();
    for (let i = 0; i < n; i++) {
      assert.ok(performance.nodeTiming.uvMetricsInfoBigInt);
    }
    bench.end(n);
  } else {
    bench.start();
    for (let i = 0; i < n; i++) {
      assert.ok(performance.nodeTiming.uvMetricsInfo);
    }
    bench.end(n);
  }
}
