'use strict';

const common = require('../common.js');
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  method: [
    'getHeapStatistics',
    'getHeapSpaceStatistics',
  ],
  n: [1e6],
});

function main({ method, n }) {
  bench.start();
  for (let i = 0; i < n; i++)
    v8[method]();
  bench.end(n);
}
