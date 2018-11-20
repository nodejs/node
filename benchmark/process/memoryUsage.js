'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [1e5]
});

function main({ n }) {
  bench.start();
  for (var i = 0; i < n; i++) {
    process.memoryUsage();
  }
  bench.end(n);
}
