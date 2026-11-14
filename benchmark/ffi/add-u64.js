'use strict';

const common = require('../common.js');
const ffi = require('node:ffi');
const { libraryPath, ensureFixtureLibrary } = require('./common.js');

const bench = common.createBenchmark(main, {
  n: [1e7],
  type: ['bigint', 'number', 'converted'],
});

ensureFixtureLibrary();

const { lib, functions } = ffi.dlopen(libraryPath, {
  add_u64: { return: 'u64', arguments: ['u64', 'u64'] },
});

const add = functions.add_u64;

function main({ n, type }) {
  const a = type === 'bigint' ? 20n : 20;
  const b = type === 'bigint' ? 22n : 22;

  if (type === 'converted') {
    bench.start();
    for (let i = 0; i < n; ++i)
      add(BigInt(a), BigInt(b));
  } else {
    bench.start();
    for (let i = 0; i < n; ++i)
      add(a, b);
  }
  bench.end(n);

  lib.close();
}
