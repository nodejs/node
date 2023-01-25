'use strict';

const common = require('../common');
const fs = require('fs');

const bench = common.createBenchmark(main, {
  n: [1e6],
  fileName: [__filename, './not-exist.txt'],
});

function main({ n, fileName }) {
  bench.start();
  for (let i = 0; i < n; i++) {
    fs.isFileSync(fileName);
  }
  bench.end(n);
}
