'use strict';

const common = require('../common');
const fs = require('fs');
const tmpdir = require('../../test/common/tmpdir');
tmpdir.refresh();

const bench = common.createBenchmark(main, {
  n: [1e4],
});

function main({ n, type }) {
  const path = tmpdir.resolve(`new-file-${process.pid}`);

  bench.start();
  for (let i = 0; i < n; i++) {
    fs.writeFileSync(path, 'Benchmark data.');
  }
  bench.end(n);
}
