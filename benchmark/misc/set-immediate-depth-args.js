'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  millions: [10]
});

function main(conf) {
  const N = +conf.millions * 1e6;

  process.on('exit', function() {
    bench.end(N / 1e6);
  });

  function cb3(n, arg2, arg3) {
    if (--n) {
      if (n % 3 === 0)
        setImmediate(cb3, n, true, null);
      else if (n % 2 === 0)
        setImmediate(cb2, n, 5.1);
      else
        setImmediate(cb1, n);
    }
  }
  function cb2(n, arg2) {
    if (--n) {
      if (n % 3 === 0)
        setImmediate(cb3, n, true, null);
      else if (n % 2 === 0)
        setImmediate(cb2, n, 5.1);
      else
        setImmediate(cb1, n);
    }
  }
  function cb1(n) {
    if (--n) {
      if (n % 3 === 0)
        setImmediate(cb3, n, true, null);
      else if (n % 2 === 0)
        setImmediate(cb2, n, 5.1);
      else
        setImmediate(cb1, n);
    }
  }
  bench.start();
  setImmediate(cb1, N);
}
