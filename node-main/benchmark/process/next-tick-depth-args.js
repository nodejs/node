'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [7e6],
});

function main({ n }) {
  let counter = n;
  function cb4(arg1, arg2, arg3, arg4) {
    if (--counter) {
      if (counter % 4 === 0)
        process.nextTick(cb4, 3.14, 1024, true, false);
      else if (counter % 3 === 0)
        process.nextTick(cb3, 512, true, null);
      else if (counter % 2 === 0)
        process.nextTick(cb2, false, 5.1);
      else
        process.nextTick(cb1, 0);
    } else
      bench.end(n);
  }

  function cb3(arg1, arg2, arg3) {
    if (--counter) {
      if (counter % 4 === 0)
        process.nextTick(cb4, 3.14, 1024, true, false);
      else if (counter % 3 === 0)
        process.nextTick(cb3, 512, true, null);
      else if (counter % 2 === 0)
        process.nextTick(cb2, false, 5.1);
      else
        process.nextTick(cb1, 0);
    } else
      bench.end(n);
  }

  function cb2(arg1, arg2) {
    if (--counter) {
      if (counter % 4 === 0)
        process.nextTick(cb4, 3.14, 1024, true, false);
      else if (counter % 3 === 0)
        process.nextTick(cb3, 512, true, null);
      else if (counter % 2 === 0)
        process.nextTick(cb2, false, 5.1);
      else
        process.nextTick(cb1, 0);
    } else
      bench.end(n);
  }

  function cb1(arg1) {
    if (--counter) {
      if (counter % 4 === 0)
        process.nextTick(cb4, 3.14, 1024, true, false);
      else if (counter % 3 === 0)
        process.nextTick(cb3, 512, true, null);
      else if (counter % 2 === 0)
        process.nextTick(cb2, false, 5.1);
      else
        process.nextTick(cb1, 0);
    } else
      bench.end(n);
  }
  bench.start();
  process.nextTick(cb1, true);
}
