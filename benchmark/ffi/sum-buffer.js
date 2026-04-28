'use strict';

const common = require('../common.js');
const ffi = require('node:ffi');
const { libraryPath, ensureFixtureLibrary } = require('./common.js');

const bench = common.createBenchmark(main, {
  size: [64, 1024, 16384],
  n: [1e6],
}, {
  flags: ['--experimental-ffi'],
});

ensureFixtureLibrary();

const { lib, functions } = ffi.dlopen(libraryPath, {
  sum_buffer: { result: 'u64', parameters: ['pointer', 'u64'] },
});

function main({ n, size }) {
  const buf = Buffer.alloc(size, 0x42);
  const ptr = ffi.getRawPointer(buf);
  const len = BigInt(size);

  const sum = functions.sum_buffer;

  bench.start();
  for (let i = 0; i < n; ++i)
    sum(ptr, len);
  bench.end(n);

  lib.close();
}
