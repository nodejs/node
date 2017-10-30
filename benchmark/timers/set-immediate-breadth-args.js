'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  millions: [5]
});

function main(conf) {
  const N = +conf.millions * 1e6;

  process.on('exit', function() {
    bench.end(N / 1e6);
  });

  function cb1(arg1) {}
  function cb2(arg1, arg2) {}
  function cb3(arg1, arg2, arg3) {}

  bench.start();
  for (let i = 0; i < N; i++) {
    if (i % 3 === 0)
      setImmediate(cb3, 512, true, null);
    else if (i % 2 === 0)
      setImmediate(cb2, false, 5.1);
    else
      setImmediate(cb1, 0);
  }
}
