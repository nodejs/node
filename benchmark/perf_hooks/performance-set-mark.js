'use strict';

const common = require('../common');
const { performance } = require('perf_hooks');

const bench = common.createBenchmark(main, {
  n: [1e6]
});

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; i++) {
    performance.mark(`${i}`);
  }
  performance.clearMarks();
  bench.end(n);
}
