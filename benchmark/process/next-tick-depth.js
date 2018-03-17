'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [12e6]
});

process.maxTickDepth = Infinity;

function main({ n }) {

  bench.start();
  process.nextTick(onNextTick);
  function onNextTick() {
    if (--n)
      process.nextTick(onNextTick);
    else
      bench.end(n);
  }
}
