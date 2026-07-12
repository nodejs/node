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
  add_f32: { return: 'f32', arguments: ['f32', 'f32'] },
});

const add = functions.add_f32;

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    add(20.5, 21.5);
  bench.end(n);

  lib.close();
}
