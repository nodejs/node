'use strict';

const common = require('../common.js');
const { randomBytes } = require('crypto');

const bench = common.createBenchmark(main, {
  size: [64, 1024, 8192, 512 * 1024],
  n: [1e3],
});

function main({ n, size }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    randomBytes(size);
  bench.end(n);
}
