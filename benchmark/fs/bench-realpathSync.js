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
    throw new Error('unknown "type": ' + type);
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
