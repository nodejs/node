'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');
const resolved_path = path.resolve(__dirname, '../../lib/');
const relative_path = path.relative(__dirname, '../../lib/');

const bench = common.createBenchmark(main, {
  n: [1e4],
  type: ['relative', 'resolved'],
});


function main(conf) {
  const n = conf.n >>> 0;
  const type = conf.type;

  bench.start();
  if (type === 'relative')
    relativePath(n);
  else if (type === 'resolved')
    resolvedPath(n);
  else
    throw new Error(`unknown "type": ${type}`);
}

function relativePath(n) {
  (function r(cntr) {
    if (cntr-- <= 0)
      return bench.end(n);
    fs.realpath(relative_path, function() {
      r(cntr);
    });
  }(n));
}

function resolvedPath(n) {
  (function r(cntr) {
    if (cntr-- <= 0)
      return bench.end(n);
    fs.realpath(resolved_path, function() {
      r(cntr);
    });
  }(n));
}
