'use strict';

const common = require('../common.js');
const cpus = require('os').cpus;

const bench = common.createBenchmark(main, {
  n: [3e4]
});

function main({ n }) {
  bench.start();
  for (var i = 0; i < n; ++i)
    cpus();
  bench.end(n);
}
