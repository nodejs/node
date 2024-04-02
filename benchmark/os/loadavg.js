'use strict';

const common = require('../common.js');
const loadavg = require('os').loadavg;

const bench = common.createBenchmark(main, {
  n: [5e6],
});

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    loadavg();
  bench.end(n);
}
