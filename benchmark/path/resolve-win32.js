'use strict';
const common = require('../common.js');
const { win32 } = require('path');

const bench = common.createBenchmark(main, {
  paths: [
    '',
    ['', ''].join('|'),
    ['c:/ignore', 'd:\\a/b\\c/d', '\\e.exe'].join('|'),
    ['c:/blah\\blah', 'd:/games', 'c:../a'].join('|')
  ],
  n: [1e6]
});

function main({ n, paths }) {
  const args = paths.split('|');

  bench.start();
  for (var i = 0; i < n; i++) {
    win32.resolve.apply(null, args);
  }
  bench.end(n);
}
