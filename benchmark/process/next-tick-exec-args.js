'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [4e6]
});

function main({ n }) {
  function onNextTick(i) {
    if (i + 1 === n)
      bench.end(n);
  }

  for (let i = 0; i < n; i++) {
    if (i % 4 === 0)
      process.nextTick(onNextTick, i, true, 10, 'test');
    else if (i % 3 === 0)
      process.nextTick(onNextTick, i, true, 10);
    else if (i % 2 === 0)
      process.nextTick(onNextTick, i, 20);
    else
      process.nextTick(onNextTick, i);
  }

  bench.start();
}
