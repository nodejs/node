'use strict';

const { createBenchmark } = require('../common.js');
const path = require('path');

const bench = createBenchmark(main, {
  n: 3e4,
});
const file = path.resolve(__dirname, '../fixtures/valid.env');

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; i++) {
    process.loadEnvFile(file);
  }
  bench.end(n);
}
