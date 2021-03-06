'use strict';
const common = require('../common.js');
const fs = require('fs');
const path = require('path');

const bench = common.createBenchmark(main, {
  value: ['@'.charCodeAt(0)],
  n: [1e6]
});

function main({ n, value }) {
  const aliceBuffer = fs.readFileSync(
    path.resolve(__dirname, '../fixtures/alice.html')
  );

  let count = 0;
  bench.start();
  for (let i = 0; i < n; i++) {
    count += aliceBuffer.indexOf(value, 0, undefined);
  }
  bench.end(n);
  return count;
}
