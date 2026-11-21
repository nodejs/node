'use strict';

const common = require('../common.js');
const assert = require('node:assert');
const { startHeapProfile } = require('node:v8');

const bench = common.createBenchmark(main, {
  n: [1e4, 1e5],
}, {
  test: { n: 1e3 },
});

function main({ n }) {
  const handle = startHeapProfile({ sampleInterval: 64 });
  // Keep the sampled allocations alive until after stop().
  const retained = globalThis.retained = new Array(n);
  for (let i = 0; i < n; i++) {
    retained[i] = new Array(16).fill(i);
  }

  bench.start();
  const profile = handle.stop();
  bench.end(1);

  assert.ok(JSON.parse(profile).samples.length > 0);
  globalThis.retained = undefined;
}
