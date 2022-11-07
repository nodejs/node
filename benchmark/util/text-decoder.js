'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  encoding: ['utf-8', 'latin1', 'iso-8859-3'],
  ignoreBOM: [0, 1],
  len: [256, 1024 * 16, 1024 * 512],
  n: [1e6]
});

function main({ encoding, len, n, ignoreBOM }) {
  const buf = Buffer.allocUnsafe(len);
  const decoder = new TextDecoder(encoding, { ignoreBOM });

  bench.start();
  for (let i = 0; i < n; i++) {
    decoder.decode(buf);
  }
  bench.end(n);
}
