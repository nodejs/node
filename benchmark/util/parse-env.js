'use strict';

const { createBenchmark } = require('../common.js');
const path = require('node:path');
const fs = require('node:fs');
const util = require('node:util');

const bench = createBenchmark(main, {
  n: 3e4,
});

const env = fs.readFileSync(path.resolve(__dirname, '../fixtures/valid.env'), 'utf-8');

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; i++) {
    util.parseEnv(env);
  }
  bench.end(n);
}
