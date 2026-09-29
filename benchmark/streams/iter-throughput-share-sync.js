'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  consumers: [2, 8, 32],
  batches: [1e4],
  n: [5],
}, {
  flags: ['--experimental-stream-iter'],
});

function main({ consumers, batches, n }) {
  const { shareSync } = require('stream/iter');
  const chunk = Buffer.alloc(1024);
  let bytes = 0;

  function* source() {
    for (let i = 0; i < batches; i++) yield [chunk];
  }

  bench.start();
  for (let run = 0; run < n; run++) {
    const shared = shareSync(source(), { budget: 65536 });
    const readers = Array.from({ length: consumers }, () =>
      shared.pull()[Symbol.iterator]());
    for (let i = 0; i < batches; i++) {
      for (let j = 0; j < consumers; j++) {
        bytes += readers[j].next().value[0].byteLength;
      }
    }
  }
  if (bytes !== batches * consumers * n * chunk.byteLength) {
    throw new Error('Incorrect byte count');
  }
  bench.end(batches * consumers * n);
}
