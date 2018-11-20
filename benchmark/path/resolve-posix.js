'use strict';
const common = require('../common.js');
const { posix } = require('path');

const bench = common.createBenchmark(main, {
  paths: [
    '',
    ['', ''].join('|'),
    ['foo/bar', '/tmp/file/', '..', 'a/../subfile'].join('|'),
    ['a/b/c/', '../../..'].join('|')
  ],
  n: [1e6]
});

function main({ n, paths }) {
  const args = paths.split('|');

  bench.start();
  for (var i = 0; i < n; i++) {
    posix.resolve.apply(null, args);
  }
  bench.end(n);
}
