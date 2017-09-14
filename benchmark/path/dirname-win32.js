'use strict';
const common = require('../common.js');
const path = require('path');

const bench = common.createBenchmark(main, {
  path: [
    '',
    '\\',
    '\\foo',
    'C:\\foo\\bar',
    'foo',
    'foo\\bar',
    'D:\\foo\\bar\\baz\\asdf\\quux'
  ],
  n: [1e6]
});

function main(conf) {
  const n = +conf.n;
  const p = path.win32;
  const input = String(conf.path);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.dirname(input);
  }
  bench.end(n);
}
