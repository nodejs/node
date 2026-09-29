'use strict';

const assert = require('assert');
const common = require('../common.js');
const { createHistogram } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e3],
  highest: [1e6, Number.MAX_SAFE_INTEGER],
  figures: [2, 3],
});

let snapshot;

function main({ n, highest, figures }) {
  const histogram = createHistogram({ highest, figures });
  for (let i = 1; i <= 1e4; i++) histogram.record(i);

  bench.start();
  for (let i = 0; i < n; i++) snapshot = histogram.snapshot();
  bench.end(n);

  assert.strictEqual(snapshot.count, 1e4);
}
