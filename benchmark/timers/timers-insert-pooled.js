'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  millions: [5],
});

function main({ millions }) {
  const iterations = millions * 1e6;

  bench.start();

  for (var i = 0; i < iterations; i++) {
    setTimeout(() => {}, 1);
  }

  bench.end(iterations / 1e6);
}
