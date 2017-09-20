'use strict';
const common = require('../common.js');
const path = require('path');

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

function main(conf) {
  const n = +conf.n;
  const p = path.win32;
  var from = String(conf.paths);
  var to = '';
  const delimIdx = from.indexOf('|');
  if (delimIdx > -1) {
    to = from.slice(delimIdx + 1);
    from = from.slice(0, delimIdx);
  }

  // Warmup
  for (var i = 0; i < n; i++) {
    p.relative(from, to);
  }

  bench.start();
  for (i = 0; i < n; i++) {
    p.relative(from, to);
  }
  bench.end(n);
}
