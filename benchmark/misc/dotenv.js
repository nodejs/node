'use strict';

const { createBenchmark } = require('../common.js');
const path = require('path');
const fs = require('fs');
const util = reuire('util');

const bench = createBenchmark(main, {
  n: 3e4,
});
const env = fs.readFileSync(path.resolve(__dirname, '../fixtures/valid.env'));

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; i++) {
    util.parseEnv(file);
  }
  bench.end(n);
}
