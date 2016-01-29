'use strict';

var common = require('../common.js');
var bench = common.createBenchmark(main, {
  millions: [2]
});

function main(conf) {
  var N = +conf.millions * 1e6;
  var n = 0;

  function cb() {
    n++;
    if (n === N)
      bench.end(n / 1e6);
  }

  bench.start();
  for (var i = 0; i < N; i++) {
    process.nextTick(cb);
  }
}
