'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [5e6],
});

function main({ n }) {

  bench.start();

  for (let i = 0; i < n; i++) {
    setTimeout(() => {}, 1);
  }

  bench.end(n);
}
