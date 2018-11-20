'use strict';
const common = require('../common.js');
const { posix } = require('path');

const bench = common.createBenchmark(main, {
  path: [
    '',
    '/',
    '/foo',
    '/foo/bar.baz',
    'foo/.bar.baz',
    'foo/bar',
    '/foo/bar/baz/asdf/.quux'
  ],
  n: [1e6]
});

function main({ n, path }) {
  for (var i = 0; i < n; i++) {
    posix.parse(path);
  }
  bench.start();
  for (i = 0; i < n; i++) {
    posix.parse(path);
  }
  bench.end(n);
}
