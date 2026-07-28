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
  pointer_to_usize: { return: 'u64', arguments: ['pointer'] },
});

const fn = functions.pointer_to_usize;
const bytes = Buffer.from([1, 2, 3, 4, 5, 6, 7, 8]);
const pointer = ffi.getRawPointer(bytes);

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    fn(pointer);
  bench.end(n);

  lib.close();
}
