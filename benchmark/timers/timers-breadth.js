'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [5e6],
});

function main({ n }) {
  var j = 0;
  bench.start();
  function cb() {
    j++;
    if (j === n)
      bench.end(n);
  }
  for (var i = 0; i < n; i++) {
    setTimeout(cb, 1);
  }
}
