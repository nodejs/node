'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [5e6]
});

function main({ n }) {
  function onNextTick(i) {
    if (i + 1 === n)
      bench.end(n);
  }

  for (var i = 0; i < n; i++) {
    process.nextTick(onNextTick, i);
  }

  bench.start();
}
