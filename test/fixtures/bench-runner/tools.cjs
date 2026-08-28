'use strict';

const { bench } = require('node:bench');

if (process.env.NODE_BENCH_PID_LOG !== undefined) {
  require('fs').appendFileSync(
    process.env.NODE_BENCH_PID_LOG, `${process.pid}\n`);
}

for (const size of [1, 2]) {
  bench('tools/simple.js', {
    params: { method: 'loop', size },
  }, (b) => {
    let value = 0;
    b.start();
    for (let i = 0; i < 1_000; i++) value += size;
    b.end(1_000);
    if (value === 0) throw new Error('unreachable');
  });
}
