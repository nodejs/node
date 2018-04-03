'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  millions: [4]
});

function main({ millions }) {
  const N = millions * 1e6;
  var n = 0;

  function cb() {
    n++;
    if (n === N)
      bench.end(n / 1e6);
  }

  bench.start();
  for (var i = 0; i < N; i++) {
    process.nextTick(cb);
  }
}
