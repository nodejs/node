'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  prefix: ['', 'node:'],
  n: [1e6],
});

function main({ prefix, n }) {
  const ids = ['fs', 'path', 'util', 'events'].map((id) => `${prefix}${id}`);
  bench.start();
  for (let i = 0; i < n; i++) {
    require(ids[i % ids.length]);
  }
  bench.end(n);
}
