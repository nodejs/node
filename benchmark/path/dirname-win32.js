'use strict';
const common = require('../common.js');
const { win32 } = require('path');

const bench = common.createBenchmark(main, {
  path: [
    '',
    '\\',
    '\\foo',
    'C:\\foo\\bar',
    'foo',
    'foo\\bar',
    'D:\\foo\\bar\\baz\\asdf\\quux',
  ],
  n: [1e6]
});

function main({ n, path }) {
  bench.start();
  for (var i = 0; i < n; i++) {
    win32.dirname(path);
  }
  bench.end(n);
}
