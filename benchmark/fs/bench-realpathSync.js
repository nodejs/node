'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');

process.chdir(__dirname);
const resolved_path = path.resolve(__dirname, '../../lib/');
const relative_path = path.relative(__dirname, '../../lib/');

const bench = common.createBenchmark(main, {
  n: [1e4],
  pathType: ['relative', 'resolved'],
});


function main({ n, pathType }) {
  bench.start();
  if (pathType === 'relative')
    relativePath(n);
  else if (pathType === 'resolved')
    resolvedPath(n);
  else
    throw new Error(`unknown "pathType": ${pathType}`);
  bench.end(n);
}

function relativePath(n) {
  for (var i = 0; i < n; i++) {
    fs.realpathSync(relative_path);
  }
}

function resolvedPath(n) {
  for (var i = 0; i < n; i++) {
    fs.realpathSync(resolved_path);
  }
}
