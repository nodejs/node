'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [5e6],
});

function main({ n }) {
  let j = 0;
  bench.start();
  function cb() {
    j++;
    if (j === n)
      bench.end(n);
  }
  for (let i = 0; i < n; i++) {
    setTimeout(cb, 1);
  }
}
