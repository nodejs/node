'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [5e6]
});

function main({ n }) {

  process.on('exit', () => {
    bench.end(n);
  });

  function cb3(n, arg2, arg3) {
    if (--n) {
      if (n % 3 === 0)
        setImmediate(cb3, n, true, null, 5.1, null, true);
      else if (n % 2 === 0)
        setImmediate(cb2, n, 5.1, true);
      else
        setImmediate(cb1, n);
    }
  }

  function cb2(n, arg2) {
    if (--n) {
      if (n % 3 === 0)
        setImmediate(cb3, n, true, null, 5.1, null, true);
      else if (n % 2 === 0)
        setImmediate(cb2, n, 5.1, true);
      else
        setImmediate(cb1, n);
    }
  }

  function cb1(n) {
    if (--n) {
      if (n % 3 === 0)
        setImmediate(cb3, n, true, null, 5.1, null, true);
      else if (n % 2 === 0)
        setImmediate(cb2, n, 5.1, true);
      else
        setImmediate(cb1, n);
    }
  }
  bench.start();
  setImmediate(cb1, n);
}
