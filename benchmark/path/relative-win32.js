'use strict';
const common = require('../common.js');
const { win32 } = require('path');

const bench = common.createBenchmark(main, {
  paths: [
    ['C:\\orandea\\test\\aaa', 'C:\\orandea\\impl\\bbb'].join('|'),
    ['C:\\', 'D:\\'].join('|'),
    ['C:\\foo\\bar\\baz', 'C:\\foo\\bar\\baz'].join('|'),
    ['C:\\foo\\BAR\\BAZ', 'C:\\foo\\bar\\baz'].join('|'),
    ['C:\\foo\\bar\\baz\\quux', 'C:\\'].join('|')
  ],
  n: [1e6]
});

function main({ n, paths }) {
  var to = '';
  const delimIdx = paths.indexOf('|');
  if (delimIdx > -1) {
    to = paths.slice(delimIdx + 1);
    paths = paths.slice(0, delimIdx);
  }

  // Warmup
  for (var i = 0; i < n; i++) {
    win32.relative(paths, to);
  }

  bench.start();
  for (i = 0; i < n; i++) {
    win32.relative(paths, to);
  }
  bench.end(n);
}
