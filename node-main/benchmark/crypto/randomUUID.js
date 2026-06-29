'use strict';

const common = require('../common.js');
const { randomUUID } = require('crypto');

const bench = common.createBenchmark(main, {
  n: [1e7],
  disableEntropyCache: [0, 1],
});

function main({ n, disableEntropyCache }) {
  disableEntropyCache = !!disableEntropyCache;
  bench.start();
  for (let i = 0; i < n; ++i)
    randomUUID({ disableEntropyCache });
  bench.end(n);
}
