'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e7],
});

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    // eslint-disable-next-line no-restricted-syntax
    new Error('test');
  bench.end(n);
}
