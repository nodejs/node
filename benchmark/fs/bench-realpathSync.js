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
  const path = pathType === 'relative' ? relative_path : resolved_path;
  bench.start();
  for (let i = 0; i < n; i++) {
    fs.realpathSync(path);
  }
  bench.end(n);
}
