'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');

const bench = common.createBenchmark(main, {
  n: [10],
  dir: [ 'lib', 'test/parallel'],
  withFileTypes: ['true', 'false']
});

function main({ n, dir, withFileTypes }) {
  withFileTypes = withFileTypes === 'true';
  const fullPath = path.resolve(__dirname, '../../', dir);
  bench.start();
  (function r(cntr) {
    if (cntr-- <= 0)
      return bench.end(n);
    fs.readdir(fullPath, { withFileTypes }, () => {
      r(cntr);
    });
  }(n));
}
