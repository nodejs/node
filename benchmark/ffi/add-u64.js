'use strict';

const common = require('../common.js');
const ffi = require('node:ffi');
const { libraryPath, ensureFixtureLibrary } = require('./common.js');

const bench = common.createBenchmark(main, {
  n: [1e7],
}, {
  flags: ['--experimental-ffi'],
});

ensureFixtureLibrary();

const { lib, functions } = ffi.dlopen(libraryPath, {
  add_u64: { return: 'u64', arguments: ['u64', 'u64'] },
});

const add = functions.add_u64;

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    add(20n, 22n);
  bench.end(n);

  lib.close();
}
