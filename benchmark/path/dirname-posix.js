'use strict';
const common = require('../common.js');
const { posix } = require('path');

const bench = common.createBenchmark(main, {
  path: [
    '',
    '/',
    '/foo',
    '/foo/bar',
    'foo',
    'foo/bar',
    '/foo/bar/baz/asdf/quux',
  ],
  n: [1e6]
});

function main({ n, path }) {
  bench.start();
  for (var i = 0; i < n; i++) {
    posix.dirname(path);
  }
  bench.end(n);
}
