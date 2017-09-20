'use strict';

const common = require('../common.js');
const v8 = require('v8');

const bench = common.createBenchmark(main, {
  method: [
    'getHeapStatistics',
    'getHeapSpaceStatistics'
  ],
  n: [1e6]
});

function main(conf) {
  const n = +conf.n;
  const method = conf.method;
  var i = 0;
  bench.start();
  for (; i < n; i++)
    v8[method]();
  bench.end(n);
}
