'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  consumers: [2, 8, 32],
  batches: [1e4],
  backpressure: ['block'],
  n: [5],
}, {
  flags: ['--experimental-stream-iter'],
});

async function main({ consumers, batches, backpressure, n }) {
  const { share, array } = require('stream/iter');
  const chunk = Buffer.alloc(1024);
  const totalOps = batches * consumers * n;

  async function* source() {
    for (let i = 0; i < batches; i++) {
      yield [chunk];
    }
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    const shared = share(source(), { highWaterMark: 64, backpressure });
    const readers = Array.from({ length: consumers }, () => array(shared.pull()));
    await Promise.all(readers);
  }
  bench.end(totalOps);
}
