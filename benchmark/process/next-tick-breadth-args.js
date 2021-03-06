'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [1e7]
});

function main({ n }) {
  let j = 0;

  function cb1(arg1) {
    j++;
    if (j === n)
      bench.end(n);
  }

  function cb2(arg1, arg2) {
    j++;
    if (j === n)
      bench.end(n);
  }

  function cb3(arg1, arg2, arg3) {
    j++;
    if (j === n)
      bench.end(n);
  }

  function cb4(arg1, arg2, arg3, arg4) {
    j++;
    if (j === n)
      bench.end(n);
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    if (i % 4 === 0)
      process.nextTick(cb4, 3.14, 1024, true, false);
    else if (i % 3 === 0)
      process.nextTick(cb3, 512, true, null);
    else if (i % 2 === 0)
      process.nextTick(cb2, false, 5.1);
    else
      process.nextTick(cb1, 0);
  }
}
