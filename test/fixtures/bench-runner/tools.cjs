'use strict';

const { completeSample } = require('../../common/bench');
const { bench } = require('node:bench');

if (process.env.NODE_BENCH_PID_LOG !== undefined) {
  require('fs').appendFileSync(
    process.env.NODE_BENCH_PID_LOG, `${process.pid}\n`);
}

for (const size of [1, 2]) {
  bench('tools/simple.js', {
    params: { method: 'loop', size },
  }, (b) => completeSample(b, 1_000));
}
