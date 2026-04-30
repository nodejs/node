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
  pointer_to_usize: { result: 'u64', parameters: ['pointer'] },
});

const fn = functions.pointer_to_usize;

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    fn(0xdeadbeefn);
  bench.end(n);

  lib.close();
}
