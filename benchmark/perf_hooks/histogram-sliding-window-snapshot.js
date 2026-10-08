'use strict';

const assert = require('assert');
const common = require('../common.js');
const { createSlidingWindowHistogram } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [100],
  chunks: [2, 8],
  recordsPerChunk: [1000],
});

let snapshot;

function main({ n, chunks, recordsPerChunk }) {
  const histogram = createSlidingWindowHistogram({
    chunks,
    recordsPerChunk,
  });
  for (let i = 0; i < chunks * recordsPerChunk; i++) {
    histogram.record((i % 1000) + 1);
  }

  bench.start();
  for (let i = 0; i < n; i++) snapshot = histogram.snapshot();
  bench.end(n);

  assert.strictEqual(snapshot.count, chunks * recordsPerChunk);
}
