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
  n: [1e5],
});

function main({ n, path }) {
  bench.start();
  for (let i = 0; i < n; i++) {
    posix.isAbsolute(i % 3 === 0 ? `${path}${i}` : path);
  }
  bench.end(n);
}
