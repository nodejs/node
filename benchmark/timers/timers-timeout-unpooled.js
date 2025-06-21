'use strict';
const common = require('../common.js');

// The following benchmark sets up n * 1e6 unpooled timeouts,
// then measures their execution on the next uv tick

const bench = common.createBenchmark(main, {
  n: [1e6],
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
    // unref().ref() will cause each of these timers to
    // allocate their own handle
    setTimeout(i % 2 ? cb : cb2, 1).unref().ref();
  }

  bench.start();
}
