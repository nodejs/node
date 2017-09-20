'use strict';
const common = require('../common.js');
const fs = require('fs');
const path = require('path');

const bench = common.createBenchmark(main, {
  value: ['@'.charCodeAt(0)],
  n: [1e7]
});

function main(conf) {
  const n = +conf.n;
  const search = +conf.value;
  const aliceBuffer = fs.readFileSync(
    path.resolve(__dirname, '../fixtures/alice.html')
  );

  bench.start();
  for (var i = 0; i < n; i++) {
    aliceBuffer.indexOf(search, 0, undefined);
  }
  bench.end(n);
}
