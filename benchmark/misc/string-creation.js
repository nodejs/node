'use strict';

var common = require('../common.js');
var bench = common.createBenchmark(main, {
  millions: [100]
});

function main(conf) {
  var n = +conf.millions * 1e6;
  bench.start();
  var s;
  for (var i = 0; i < n; i++) {
    s = '01234567890';
    s[1] = 'a';
  }
  bench.end(n / 1e6);
}
