'use strict';

const common = require('../common');
const Readable = require('stream').Readable;

const bench = common.createBenchmark(main, {
  n: [1e7],
});

async function main({ n }) {
  const arr = [];
  for (let i = 0; i < n; i++) {
    arr.push(`${i}`);
  }

  const s = new Readable.from(arr);

  bench.start();
  s.on('data', (data) => {
    data;
  });
  s.on('close', () => {
    bench.end(n);
  });

}
