'use strict';

const common = require('../common.js');
const { randomUUID } = require('crypto');

const bench = common.createBenchmark(main, {
  n: [1e7],
  disableEntropyCache: [false, true],
});

function main({ n, disableEntropyCache }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    randomUUID({ disableEntropyCache });
  bench.end(n);
}
