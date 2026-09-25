'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');

const bench = common.createBenchmark(main, {
  n: [10],
  dir: [ 'lib', 'test/parallel'],
  withFileTypes: ['true', 'false'],
});


function main({ n, dir, withFileTypes }) {
  withFileTypes = withFileTypes === 'true';
  const fullPath = path.resolve(__dirname, '../../', dir);
  bench.start();
  for (let i = 0; i < n; i++) {
    fs.readdirSync(fullPath, { withFileTypes });
  }
  bench.end(n);
}
