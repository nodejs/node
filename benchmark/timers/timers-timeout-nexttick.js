'use strict';
const common = require('../common.js');

// The following benchmark measures setting up n * 1e6 timeouts,
// as well as scheduling a next tick from each timeout. Those
// then get executed on the next uv tick.

const bench = common.createBenchmark(main, {
  n: [5e4, 5e6],
});

function main({ n }) {
  let count = 0;

  // Function tracking on the hidden class in V8 can cause misleading
  // results in this benchmark if only a single function is used —
  // alternate between two functions for a fairer benchmark.

  function cb() {
    process.nextTick(counter);
  }
  function cb2() {
    process.nextTick(counter);
  }
  function counter() {
    count++;
    if (count === n)
      bench.end(n);
  }

  for (var i = 0; i < n; i++) {
    setTimeout(i % 2 ? cb : cb2, 1);
  }

  bench.start();
}
