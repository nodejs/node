'use strict';
const common = require('../common.js');
const { AsyncLocalStorage } = require('async_hooks');

/**
 * This benchmark verifies the performance of `AsyncLocalStorage.run()`
 * nested inside multiple active `AsyncLocalStorage.run()` scopes.
 *
 * - AsyncLocalStorage1.run()
 *   - AsyncLocalStorage2.run()
 *    ...
 *      - AsyncLocalStorageN.run()
 *        - store.run(i, fn)
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

function noop() {}

function main({ n, storageCount }) {
  const stores = new Array(storageCount).fill(0).map(() => new AsyncLocalStorage());
  const target = new AsyncLocalStorage();
  const contextValue = {};

  runStores(stores, contextValue, () => {
    bench.start();
    for (let i = 0; i < n; i++) {
      target.run(i, noop);
    }
    bench.end(n);
  });
}
