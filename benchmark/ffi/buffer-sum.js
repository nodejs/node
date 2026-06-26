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
  sum_buffer: { return: 'u64', arguments: ['buffer', 'u64'] },
});

const fn = functions.sum_buffer;
const bytes = Buffer.from([1, 2, 3, 4, 5, 6, 7, 8]);
const length = BigInt(bytes.length);

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    fn(bytes, length);
  bench.end(n);

  lib.close();
}
