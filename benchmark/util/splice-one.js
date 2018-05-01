'use strict';

const common = require('../common');

const bench = common.createBenchmark(main, {
  n: [1e7],
  size: [10, 100, 500],
}, { flags: ['--expose-internals'] });

function main({ n, size, type }) {
  const { spliceOne } = require('internal/util');
  const arr = new Array(size);
  arr.fill('');
  const pos = Math.floor(size / 2);

  bench.start();
  for (var i = 0; i < n; i++) {
    spliceOne(arr, pos);
    arr.push('');
  }
  bench.end(n);
}
