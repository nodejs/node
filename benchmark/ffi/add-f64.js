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
  add_f64: { result: 'f64', parameters: ['f64', 'f64'] },
});

const add = functions.add_f64;

function main({ n }) {
  bench.start();
  for (let i = 0; i < n; ++i)
    add(1.5, 2.5);
  bench.end(n);

  lib.close();
}
