'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  millions: [12]
});

process.maxTickDepth = Infinity;

function main(conf) {
  var n = +conf.millions * 1e6;

  bench.start();
  process.nextTick(onNextTick);
  function onNextTick() {
    if (--n)
      process.nextTick(onNextTick);
    else
      bench.end(+conf.millions);
  }
}
