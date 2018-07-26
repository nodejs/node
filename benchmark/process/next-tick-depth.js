'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [12e6]
});

function main({ n }) {
  let counter = n;
  bench.start();
  process.nextTick(onNextTick);
  function onNextTick() {
    if (--counter)
      process.nextTick(onNextTick);
    else
      bench.end(n);
  }
}
