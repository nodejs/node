'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1e6],
});

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; i++) {
    const ac = new AbortController();
    ac.signal.addEventListener('abort', () => {});
    ac.abort();
  }
  bench.end(n);
}
