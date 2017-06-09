'use strict';
var common = require('../common.js');

var bench = common.createBenchmark(main, {
  thousands: [500],
});

function main(conf) {
  var iterations = +conf.thousands * 1e3;

  bench.start();

  for (var i = 0; i < iterations; i++) {
    setTimeout(() => {}, 1);
  }

  bench.end(iterations / 1e3);
}
