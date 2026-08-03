'use strict';
const common = require('../common.js');
const { AsyncLocalStorage } = require('async_hooks');

const bench = common.createBenchmark(main, {
  n: [1e7],
});

async function run(store, n) {
  for (let i = 0; i < n; i++) {
    await new Promise((resolve) => store.run(i, resolve));
  }
}

function main({ n }) {
  const store = new AsyncLocalStorage();
  bench.start();
  run(store, n).then(() => {
    bench.end(n);
  });
}
