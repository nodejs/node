'use strict';

const common = require('../common.js');
const networkInterfaces = require('os').networkInterfaces;

const bench = common.createBenchmark(main, {
  n: [1e4]
});

function main({ n }) {
  bench.start();
  for (var i = 0; i < n; ++i)
    networkInterfaces();
  bench.end(n);
}
