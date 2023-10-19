'use strict';
const common = require('../common.js');
const { win32 } = require('path');

const bench = common.createBenchmark(main, {
  paths: [
    '',
    ['', ''].join('|'),
    ['c:/ignore', 'd:\\a/b\\c/d', '\\e.exe'].join('|'),
    ['c:/blah\\blah', 'd:/games', 'c:../a'].join('|'),
  ],
  n: [1e5],
});

function main({ n, paths }) {
  const args = paths.split('|');
  const copy = [...args];
  const orig = copy[0];

  bench.start();
  for (let i = 0; i < n; i++) {
    if (i % 3 === 0) {
      copy[0] = `${orig}${i}`;
      win32.resolve(...copy);
    } else {
      win32.resolve(...args);
    }
  }
  bench.end(n);
}
