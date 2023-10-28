// Creation benchmark
// creates a single hasher, then pushes a minimal data through it
'use strict';
const common = require('../common.js');
const crypto = require('crypto');

const bench = common.createBenchmark(main, {
  n: [10e3],
  algo: ['md5' ],
});

const buf = Buffer.alloc(4);

function main({ algo, n }) {
  bench.start();
  for (let i = 0; i < n; ++i) {
    const h = crypto.createHash(algo);
    h.end(buf);
  }
  bench.end(n);
}
