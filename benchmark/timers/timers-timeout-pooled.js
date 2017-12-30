'use strict';
const common = require('../common.js');

// The following benchmark measures setting up n * 1e6 timeouts,
// which then get executed on the next uv tick

const bench = common.createBenchmark(main, {
  millions: [10],
});

function main({ millions }) {
  const iterations = millions * 1e6;
  let count = 0;

  // Function tracking on the hidden class in V8 can cause misleading
  // results in this benchmark if only a single function is used —
  // alternate between two functions for a fairer benchmark

  function cb() {
    count++;
    if (count === iterations)
      bench.end(iterations / 1e6);
  }
  function cb2() {
    count++;
    if (count === iterations)
      bench.end(iterations / 1e6);
  }

  for (var i = 0; i < iterations; i++) {
    setTimeout(i % 2 ? cb : cb2, 1);
  }

  bench.start();
}
