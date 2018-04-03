'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  millions: [12]
});

process.maxTickDepth = Infinity;

function main({ millions }) {
  var n = millions * 1e6;

  bench.start();
  process.nextTick(onNextTick);
  function onNextTick() {
    if (--n)
      process.nextTick(onNextTick);
    else
      bench.end(millions);
  }
}
