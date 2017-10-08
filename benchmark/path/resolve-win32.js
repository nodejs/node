'use strict';
const common = require('../common.js');
const path = require('path');

const bench = common.createBenchmark(main, {
  paths: [
    '',
    ['', ''].join('|'),
    ['c:/ignore', 'd:\\a/b\\c/d', '\\e.exe'].join('|'),
    ['c:/blah\\blah', 'd:/games', 'c:../a'].join('|')
  ],
  n: [1e6]
});

function main(conf) {
  const n = +conf.n;
  const p = path.win32;
  const args = String(conf.paths).split('|');

  bench.start();
  for (var i = 0; i < n; i++) {
    p.resolve.apply(null, args);
  }
  bench.end(n);
}
