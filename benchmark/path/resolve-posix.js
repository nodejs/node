'use strict';
const common = require('../common.js');
const path = require('path');

const bench = common.createBenchmark(main, {
  paths: [
    '',
    ['', ''].join('|'),
    ['foo/bar', '/tmp/file/', '..', 'a/../subfile'].join('|'),
    ['a/b/c/', '../../..'].join('|')
  ],
  n: [1e6]
});

function main(conf) {
  const n = +conf.n;
  const p = path.posix;
  const args = String(conf.paths).split('|');

  bench.start();
  for (var i = 0; i < n; i++) {
    p.resolve.apply(null, args);
  }
  bench.end(n);
}
