'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e3],
});

function main({ n }) {
  let i = 0;
  bench.start();
  setTimeout(cb, 1);
  function cb() {
    i++;
    if (i === n)
      bench.end(n);
    else
      setTimeout(cb, 1);
  }
}
