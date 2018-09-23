'use strict';

var common = require('../common.js');
var bench = common.createBenchmark(main, {
  millions: [2]
});

process.maxTickDepth = Infinity;

function main(conf) {
  var n = +conf.millions * 1e6;

  function cb3(arg1, arg2, arg3) {
    if (--n) {
      if (n % 3 === 0)
        process.nextTick(cb3, 512, true, null);
      else if (n % 2 === 0)
        process.nextTick(cb2, false, 5.1);
      else
        process.nextTick(cb1, 0);
    } else
      bench.end(+conf.millions);
  }
  function cb2(arg1, arg2) {
    if (--n) {
      if (n % 3 === 0)
        process.nextTick(cb3, 512, true, null);
      else if (n % 2 === 0)
        process.nextTick(cb2, false, 5.1);
      else
        process.nextTick(cb1, 0);
    } else
      bench.end(+conf.millions);
  }
  function cb1(arg1) {
    if (--n) {
      if (n % 3 === 0)
        process.nextTick(cb3, 512, true, null);
      else if (n % 2 === 0)
        process.nextTick(cb2, false, 5.1);
      else
        process.nextTick(cb1, 0);
    } else
      bench.end(+conf.millions);
  }
  bench.start();
  process.nextTick(cb1, true);
}
