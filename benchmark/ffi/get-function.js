'use strict';

// Measures repeated getFunction() calls with a cached callable rather than
// call throughput. The warmup populates the cache for the measured symbol.
// See get-function-cache-miss.js for first-time resolution, including trampoline
// creation for fast-eligible signatures on supported platforms.

const common = require('../common.js');
const { DynamicLibrary } = require('node:ffi');
const { libraryPath, ensureFixtureLibrary } = require('./common.js');

const bench = common.createBenchmark(main, {
  signature: ['fast', 'slow'],
  n: [1e3],
});

ensureFixtureLibrary();

const signatures = {
  fast: { name: 'add_i32', return: 'i32', arguments: ['i32', 'i32'] },
  slow: {
    name: 'sum_8_i32',
    return: 'i32',
    arguments: ['i32', 'i32', 'i32', 'i32', 'i32', 'i32', 'i32', 'i32'],
  },
};

function main({ n, signature }) {
  const { name, ...definition } = signatures[signature];
  const lib = new DynamicLibrary(libraryPath);

  // Populate the callable cache before measuring repeated lookups.
  lib.getFunction(name, definition);

  bench.start();
  for (let i = 0; i < n; ++i)
    lib.getFunction(name, definition);
  bench.end(n);

  lib.close();
}
