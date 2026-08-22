'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');

const bench = common.createBenchmark(main, {
  n: [10],
  dir: [ 'lib', 'test/parallel'],
  withFileTypes: ['true', 'false'],
  recursive: ['true', 'false'],
});


function main({ n, dir, withFileTypes, recursive }) {
  withFileTypes = withFileTypes === 'true';
  recursive = recursive === 'true';
  const fullPath = path.resolve(__dirname, '../../', dir);
  bench.start();
  for (let i = 0; i < n; i++) {
    fs.readdirSync(fullPath, { withFileTypes, recursive });
  }
  bench.end(n);
}
