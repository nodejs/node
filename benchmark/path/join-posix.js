'use strict';
const common = require('../common.js');
const path = require('path');

const bench = common.createBenchmark(main, {
  paths: [
    ['/foo', 'bar', '', 'baz/asdf', 'quux', '..'].join('|')
  ],
  n: [1e6]
});

function main(conf) {
  const n = +conf.n;
  const p = path.posix;
  const args = String(conf.paths).split('|');

  bench.start();
  for (var i = 0; i < n; i++) {
    p.join.apply(null, args);
  }
  bench.end(n);
}
