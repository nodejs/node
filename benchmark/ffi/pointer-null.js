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
  is_null_pointer: { return: 'u8', arguments: ['pointer'] },
});

const fn = functions.is_null_pointer;

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    fn(null);
  bench.end(n);

  lib.close();
}
