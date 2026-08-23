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
  string_equals_hello: { return: 'u8', arguments: ['pointer'] },
});

const fn = functions.string_equals_hello;
const string = Buffer.from('hello\0');
const pointer = ffi.getRawPointer(string);

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    fn(pointer);
  bench.end(n);

  lib.close();
}
