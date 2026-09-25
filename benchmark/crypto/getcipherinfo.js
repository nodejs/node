'use strict';

const common = require('../common.js');
const { getCiphers, getCipherInfo } = require('node:crypto');
const bench = common.createBenchmark(main, {
  cipher: getCiphers(),
  n: 50,
});

function main({ n, cipher }) {
  bench.start();
  for (let i = 0; i < n; i++) {
    getCipherInfo(cipher);
  }
  bench.end(n);
}
