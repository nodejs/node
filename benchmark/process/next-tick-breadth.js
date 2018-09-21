'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [4e6]
});

function main({ n }) {
  var j = 0;

  function cb() {
    j++;
    if (j === n)
      bench.end(n);
  }

  bench.start();
  for (var i = 0; i < n; i++) {
    process.nextTick(cb);
  }
}
