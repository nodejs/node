'use strict';
const common = require('../common.js');

// The following benchmark sets up n * 1e6 unpooled timeouts,
// then measures their execution on the next uv tick

const bench = common.createBenchmark(main, {
  millions: [1],
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
    // unref().ref() will cause each of these timers to
    // allocate their own handle
    setTimeout(i % 2 ? cb : cb2, 1).unref().ref();
  }

  bench.start();
}
