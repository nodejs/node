'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [1e7],
});

function main({ n }) {

  process.on('exit', () => {
    bench.end(n);
  });

  function cb() {}

  bench.start();
  for (let i = 0; i < n; i++) {
    setImmediate(cb);
  }
}
