'use strict';
const common = require('../common.js');
const { posix } = require('path');

const bench = common.createBenchmark(main, {
  paths: [
    ['/foo', 'bar', '', 'baz/asdf', 'quux', '..'].join('|'),
  ],
  n: [1e6]
});

function main({ n, paths }) {
  const args = paths.split('|');

  bench.start();
  for (var i = 0; i < n; i++) {
    posix.join.apply(null, args);
  }
  bench.end(n);
}
