'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  len: [0, 1, 64, 1024],
  n: [1e7]
});

function main({ len, n }) {
  const buf = Buffer.alloc(len);

  for (let i = 0; i < buf.length; i++)
    buf[i] = i & 0xff;

  const hex = buf.toString('hex');

  bench.start();

  for (let i = 0; i < n; i += 1)
    Buffer.from(hex, 'hex');

  bench.end(n);
}
