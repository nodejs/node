'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [12e5]
});

function main({ n }) {
  let counter = n;
  bench.start();
  queueMicrotask(onNextTick);
  function onNextTick() {
    if (--counter)
      queueMicrotask(onNextTick);
    else
      bench.end(n);
  }
}
