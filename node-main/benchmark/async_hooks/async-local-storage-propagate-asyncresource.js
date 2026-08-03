'use strict';
const common = require('../common.js');
const { AsyncLocalStorage, AsyncResource } = require('async_hooks');

/**
 * This benchmark verifies the performance degradation of
 * async resource propagation on the increasing number of
 * active `AsyncLocalStorage`s.
 *
 * - AsyncLocalStorage.run() * storageCount
 * - new AsyncResource()
 * - new AsyncResource()
 * ...
 * - N new Asyncresource()
 */
const bench = common.createBenchmark(main, {
  storageCount: [0, 1, 10, 100],
  n: [1e3],
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

function runBenchmark(n) {
  for (let i = 0; i < n; i++) {
    new AsyncResource('noop');
  }
}

function main({ n, storageCount }) {
  const stores = new Array(storageCount).fill(0).map(() => new AsyncLocalStorage());
  const contextValue = {};

  runStores(stores, contextValue, () => {
    bench.start();
    runBenchmark(n);
    bench.end(n);
  });
}
