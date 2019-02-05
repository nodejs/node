'use strict';
const common = require('../common.js');
const { win32 } = require('path');

const bench = common.createBenchmark(main, {
  paths: [
    ['C:\\foo', 'bar', '', 'baz\\asdf', 'quux', '..'].join('|'),
  ],
  n: [1e6]
});

function main({ n, paths }) {
  const args = paths.split('|');

  bench.start();
  for (var i = 0; i < n; i++) {
    win32.join.apply(null, args);
  }
  bench.end(n);
}
