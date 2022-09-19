'use strict';

const common = require('../common.js');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  n: [60e4]
});

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    fs.readFileSync(__filename);
  bench.end(n);
}
