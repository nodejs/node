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
  sum_5_i32: { return: 'i32', arguments: ['i32', 'i32', 'i32', 'i32', 'i32'] },
});

const fn = functions.sum_5_i32;

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    fn(2, 4, 8, 12, 16);
  bench.end(n);

  lib.close();
}
