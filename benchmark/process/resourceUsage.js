'use strict';

var common = require('../common.js');
var bench = common.createBenchmark(main, {
  n: [1e6]
});

function main(conf) {
  var n = +conf.n;

  bench.start();
  for (var i = 0; i < n; i++) {
    process.resourceUsage();
  }
  bench.end(n);
}
