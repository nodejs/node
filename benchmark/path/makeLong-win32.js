'use strict';
const common = require('../common.js');
const { win32 } = require('path');

const bench = common.createBenchmark(main, {
  path: [
    'foo\\bar',
    'C:\\foo',
    '\\\\foo\\bar',
    '\\\\?\\foo'
  ],
  n: [1e6]
});

function main({ n, path }) {
  bench.start();
  for (var i = 0; i < n; i++) {
    win32._makeLong(path);
  }
  bench.end(n);
}
