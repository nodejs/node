'use strict';

const { createBenchmark } = require('../common.js');
const path = require('path');

const bench = createBenchmark(main, {
  n: 3e4,
});

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; i++) {
    process.loadEnvFile(path.resolve(__dirname, '../fixtures/valid.env'));
  }
  bench.end(n);
}
