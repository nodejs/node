'use strict';
var common = require('../common.js');

var bench = common.createBenchmark(main, {
  thousands: [1],
});

function main(conf) {
  var N = +conf.thousands * 1e3;
  var n = 0;
  bench.start();
  setTimeout(cb, 1);
  function cb() {
    n++;
    if (n === N)
      bench.end(N / 1e3);
    else
      setTimeout(cb, 1);
  }
}
