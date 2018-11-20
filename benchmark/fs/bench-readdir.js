'use strict';

const common = require('../common');
const fs = require('fs');
const path = require('path');

const bench = common.createBenchmark(main, {
  n: [1e4],
});


function main({ n }) {
  bench.start();
  (function r(cntr) {
    if (cntr-- <= 0)
      return bench.end(n);
    fs.readdir(path.resolve(__dirname, '../../lib/'), function() {
      r(cntr);
    });
  }(n));
}
