'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  urls: [1, 100],
  n: [1e7],
}, {
  flags: ['--expose-internals'],
});

function main({ urls, n }) {
  const { LoadCache } = require('internal/modules/esm/module_map');
  const cache = new LoadCache();
  const job = () => {};
  const keys = Array.from({ length: urls }, (_, i) => `file:///module-${i}.mjs`);
  for (let i = 0; i < keys.length; i++) cache.set(keys[i], undefined, job);

  bench.start();
  for (let i = 0; i < n; i++) {
    if (cache.get(keys[i % keys.length]) !== job) {
      throw new Error('Load cache miss');
    }
  }
  bench.end(n);
}
