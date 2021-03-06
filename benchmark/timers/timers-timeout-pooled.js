'use strict';
const common = require('../common.js');

// The following benchmark measures setting up n * 1e6 timeouts,
// which then get executed on the next uv tick

const bench = common.createBenchmark(main, {
  n: [1e7],
});

function main({ n }) {
  let count = 0;

  // Function tracking on the hidden class in V8 can cause misleading
  // results in this benchmark if only a single function is used —
  // alternate between two functions for a fairer benchmark

  function cb() {
    count++;
    if (count === n)
      bench.end(n);
  }

  function cb2() {
    count++;
    if (count === n)
      bench.end(n);
  }

  for (let i = 0; i < n; i++) {
    setTimeout(i % 2 ? cb : cb2, 1);
  }

  bench.start();
}
