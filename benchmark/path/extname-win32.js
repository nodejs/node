'use strict';
const common = require('../common.js');
const { win32 } = require('path');

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
    '\\foo\\bar\\baz\\asdf\\quux.foobarbazasdfquux',
  ],
  n: [1e6]
});

function main({ n, path }) {
  bench.start();
  for (var i = 0; i < n; i++) {
    win32.extname(path);
  }
  bench.end(n);
}
