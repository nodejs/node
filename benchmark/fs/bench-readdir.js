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
  bench.start();
  (function r(cntr) {
    if (cntr-- <= 0)
      return bench.end(n);
    const fullPath = path.resolve(__dirname, '../../', dir);
    fs.readdir(fullPath, { withFileTypes }, function() {
      r(cntr);
    });
  }(n));
}
