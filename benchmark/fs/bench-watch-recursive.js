'use strict';

// Setting up (and tearing down) a recursive fs.watch() on a directory tree.
// On Linux and other platforms without a native recursive watcher this is
// implemented in JavaScript on top of per-directory watchers.

const common = require('../common');
const fs = require('fs');
const path = require('path');

const bench = common.createBenchmark(main, {
  n: [5],
  dir: ['lib', 'test/fixtures'],
});

function main({ n, dir }) {
  const fullPath = path.resolve(__dirname, '../../', dir);
  bench.start();
  for (let i = 0; i < n; i++) {
    fs.watch(fullPath, { recursive: true }).close();
  }
  bench.end(n);
}
