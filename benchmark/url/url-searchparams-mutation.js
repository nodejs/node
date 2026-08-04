'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['set', 'delete'],
  type: ['unique', 'duplicates'],
  count: [10, 1000],
  n: [1e4],
});

function buildSeed(type, count) {
  const parts = new Array(count);

  if (type === 'duplicates') {
    for (let i = 0; i < count; i++) {
      parts[i] = `dup=${i}`;
    }
  } else {
    for (let i = 0; i < count; i++) {
      parts[i] = `k${i}=${i}`;
    }
  }

  return new URLSearchParams(parts.join('&'));
}

function main({ method, type, count, n }) {
  const seed = buildSeed(type, count);
  const key = type === 'duplicates' ? 'dup' : 'k0';
  const mutate = method === 'set' ?
    (params) => params.set(key, 'updated') :
    (params) => params.delete(key);

  for (let i = 0; i < 1e3; i++) {
    mutate(new URLSearchParams(seed));
  }

  bench.start();
  for (let i = 0; i < n; i++) {
    mutate(new URLSearchParams(seed));
  }
  bench.end(n);
}
