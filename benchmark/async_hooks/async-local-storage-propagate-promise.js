'use strict';
const common = require('../common.js');
const { AsyncLocalStorage } = require('async_hooks');

/**
 * This benchmark verifies the performance degradation of
 * async resource propagation on the increasing number of
 * active `AsyncLocalStorage`s.
 *
 * - AsyncLocalStorage.run()
 *  - Promise
 *    - Promise
 *      ...
 *        - Promise
 */
const bench = common.createBenchmark(main, {
  storageCount: [0, 1, 10, 100],
  n: [1e5],
});

function runStores(stores, value, cb, idx = 0) {
  if (idx === stores.length) {
    cb();
  } else {
    stores[idx].run(value, () => {
      runStores(stores, value, cb, idx + 1);
    });
  }
}

async function runBenchmark(n) {
  for (let i = 0; i < n; i++) {
    // Avoid creating additional ticks.
    await undefined;
  }
}

function main({ n, storageCount }) {
  const stores = new Array(storageCount).fill(0).map(() => new AsyncLocalStorage());
  const contextValue = {};

  runStores(stores, contextValue, () => {
    bench.start();
    runBenchmark(n).then(() => {
      bench.end(n);
    });
  });
}
