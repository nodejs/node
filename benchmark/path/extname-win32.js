'use strict';
const common = require('../common.js');
const path = require('path');

const bench = common.createBenchmark(main, {
  path: [
    '',
    '\\',
    'C:\\foo',
    'foo\\.bar.baz',
    'index.html',
    'index',
    'foo\\bar\\..baz.quux',
    'foo\\bar\\...baz.quux',
    'D:\\foo\\bar\\baz\\asdf\\quux',
    '\\foo\\bar\\baz\\asdf\\quux.foobarbazasdfquux'
  ],
  n: [1e6]
});

function main(conf) {
  const n = +conf.n;
  const p = path.win32;
  const input = String(conf.path);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.extname(input);
  }
  bench.end(n);
}
