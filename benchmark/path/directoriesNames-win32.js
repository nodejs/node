'use strict';
const common = require('../common.js');
const { win32 } = require('path');

const bench = common.createBenchmark(main, {
  pathext: [
    'C:\\',
    'D:\\DDL\\ANIME\\les chevaliers du zodiaques\\whateverFile.avi'
  ],
  n: [1e6]
});

function main({ n, pathext }) {
  bench.start();
  for (var i = 0; i < n; i++) {
    win32.directoriesNames(pathext);
  }
  bench.end(n);
}
