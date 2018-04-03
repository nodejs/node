'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  millions: [5]
});

function main({ millions }) {
  var n = millions * 1e6;

  bench.start();
  for (var i = 0; i < n; i++) {
    if (i % 4 === 0)
      process.nextTick(onNextTick, i, true, 10, 'test');
    else if (i % 3 === 0)
      process.nextTick(onNextTick, i, true, 10);
    else if (i % 2 === 0)
      process.nextTick(onNextTick, i, 20);
    else
      process.nextTick(onNextTick, i);
  }
  function onNextTick(i) {
    if (i + 1 === n)
      bench.end(millions);
  }
}
