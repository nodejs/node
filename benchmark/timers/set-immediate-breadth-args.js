'use strict';

const common = require('../common.js');
const bench = common.createBenchmark(main, {
  n: [5e6]
});

function main({ n }) {

  process.on('exit', () => {
    bench.end(n);
  });

  function cb1(arg1) {}
  function cb2(arg1, arg2) {}
  function cb3(arg1, arg2, arg3) {}

  bench.start();
  for (let i = 0; i < n; i++) {
    if (i % 3 === 0)
      setImmediate(cb3, 512, true, null, 512, true, null);
    else if (i % 2 === 0)
      setImmediate(cb2, false, 5.1, 512);
    else
      setImmediate(cb1, 0);
  }
}
