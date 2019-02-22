'use strict';
const common = require('../common.js');
const { win32 } = require('path');

const bench = common.createBenchmark(main, {
  path: [
    '',
    '.',
    '//server',
    'C:\\baz\\..',
    'C:baz\\..',
    'bar\\baz',
  ],
  n: [1e6]
});

function main({ n, path }) {
  bench.start();
  for (var i = 0; i < n; i++) {
    win32.isAbsolute(path);
  }
  bench.end(n);
}
