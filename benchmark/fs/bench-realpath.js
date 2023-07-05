'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');
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
  else
    resolvedPath(n);
}

function relativePath(n) {
  (function r(cntr) {
    if (cntr-- <= 0)
      return bench.end(n);
    fs.realpath(relative_path, () => {
      r(cntr);
    });
  }(n));
}

function resolvedPath(n) {
  (function r(cntr) {
    if (cntr-- <= 0)
      return bench.end(n);
    fs.realpath(resolved_path, () => {
      r(cntr);
    });
  }(n));
}
