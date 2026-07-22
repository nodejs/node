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
  first_byte: { return: 'u8', arguments: ['buffer'] },
});

const fn = functions.first_byte;
const bytes = Buffer.from([1, 2, 3, 4, 5, 6, 7, 8]);

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    fn(bytes);
  bench.end(n);

  lib.close();
}
