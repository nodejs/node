'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  millions: [10],
});

function main({ millions }) {
  const N = millions * 1e6;

  process.on('exit', function() {
    bench.end(millions);
  });

  function cb() {}

  bench.start();
  for (let i = 0; i < N; i++) {
    setImmediate(cb);
  }
}
