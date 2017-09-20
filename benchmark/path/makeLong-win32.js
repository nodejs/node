'use strict';
const common = require('../common.js');
const path = require('path');

const bench = common.createBenchmark(main, {
  path: [
    'foo\\bar',
    'C:\\foo',
    '\\\\foo\\bar',
    '\\\\?\\foo'
  ],
  n: [1e6]
});

function main(conf) {
  const n = +conf.n;
  const p = path.win32;
  const input = String(conf.path);

  bench.start();
  for (var i = 0; i < n; i++) {
    p._makeLong(input);
  }
  bench.end(n);
}
