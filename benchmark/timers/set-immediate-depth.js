'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  millions: [10]
});

function main(conf) {
  const N = +conf.millions * 1e6;
  let n = N;

  process.on('exit', function() {
    bench.end(N / 1e6);
  });

  bench.start();
  setImmediate(onNextTick);
  function onNextTick() {
    if (--n)
      setImmediate(onNextTick);
  }
}
