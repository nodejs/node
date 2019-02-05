'use strict';
const common = require('../common.js');
const { win32 } = require('path');

const bench = common.createBenchmark(main, {
  path: [
    '',
    'C:\\',
    'C:\\foo',
    '\\foo',
    'E:\\foo\\bar.baz',
    'foo\\.bar.baz',
    'foo\\bar',
    '\\foo\\bar\\baz\\asdf\\.quux',
  ],
  n: [1e6]
});

function main({ n, path }) {
  for (var i = 0; i < n; i++) {
    win32.parse(path);
  }
  bench.start();
  for (i = 0; i < n; i++) {
    win32.parse(path);
  }
  bench.end(n);
}
