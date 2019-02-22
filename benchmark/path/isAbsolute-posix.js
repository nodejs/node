'use strict';
const common = require('../common.js');
const { posix } = require('path');

const bench = common.createBenchmark(main, {
  path: [
    '',
    '.',
    '/foo/bar',
    '/baz/..',
    'bar/baz',
  ],
  n: [1e6]
});

function main({ n, path }) {
  bench.start();
  for (var i = 0; i < n; i++) {
    posix.isAbsolute(path);
  }
  bench.end(n);
}
