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
  string_length: { return: 'u64', arguments: ['buffer'] },
});

const fn = functions.string_length;
const string = Buffer.from('hello\0');

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    fn(string);
  bench.end(n);

  lib.close();
}
