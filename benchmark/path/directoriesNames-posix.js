'use strict';
const common = require('../common.js');
const { posix } = require('path');

const bench = common.createBenchmark(main, {
  pathext: [
    '/',
    'D:/DDL/EBOOK/whatEverFile.pub',
    '/foo'
  ],
  n: [1e6]
});

function main({ n, pathext }) {
  bench.start();
  for (var i = 0; i < n; i++) {
    posix.directoriesNames(pathext);
  }
  bench.end(n);
}
