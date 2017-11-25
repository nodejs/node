'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  millions: [10],
});

function main(conf) {
  const iterations = +conf.millions * 1e6;

  // Function tracking on the hidden class in V8 can cause misleading
  // results in this benchmark if only a single function is used —
  // alternate between two functions for a fairer benchmark

  function cb() {}
  function cb2() {}

  process.on('exit', function() {
    bench.end(iterations / 1e6);
  });

  for (var i = 0; i < iterations; i++) {
    setTimeout(i % 2 ? cb : cb2, 1);
  }

  bench.start();
}
