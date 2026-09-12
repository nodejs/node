'use strict';

const assert = require('assert');
const common = require('../common.js');
const { createSlidingWindowHistogram } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e6],
  mode: ['count', 'time'],
  chunks: [6],
});

function main({ n, mode, chunks }) {
  const options = mode === 'count' ?
    { chunks, recordsPerChunk: 1000 } :
    { chunks, chunkDuration: 1 };
  const histogram = createSlidingWindowHistogram(options);

  bench.start();
  for (let i = 0; i < n; i++) histogram.record((i % 1000) + 1);
  bench.end(n);

  assert.ok(histogram.snapshot().count > 0);
}
