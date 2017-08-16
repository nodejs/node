'use strict';

var common = require('../common.js');
var bench = common.createBenchmark(main, {
  millions: [2]
});

function main(conf) {
  var N = +conf.millions * 1e6;
  var n = 0;

  function cb1(arg1) {
    n++;
    if (n === N)
      bench.end(n / 1e6);
  }
  function cb2(arg1, arg2) {
    n++;
    if (n === N)
      bench.end(n / 1e6);
  }
  function cb3(arg1, arg2, arg3) {
    n++;
    if (n === N)
      bench.end(n / 1e6);
  }
  function cb4(arg1, arg2, arg3, arg4) {
    n++;
    if (n === N)
      bench.end(n / 1e6);
  }

  bench.start();
  for (var i = 0; i < N; i++) {
    if (i % 4 === 0)
      process.nextTick(cb4, 3.14, 1024, true, false);
    else if (i % 3 === 0)
      process.nextTick(cb3, 512, true, null);
    else if (i % 2 === 0)
      process.nextTick(cb2, false, 5.1);
    else
      process.nextTick(cb1, 0);
  }
}
