'use strict';

// Measures symbol resolution rather than call throughput. Creating a callable
// for a fast-eligible signature emits a native trampoline, so this benchmark
// covers the trampoline allocation path that the call benchmarks never reach.
//
// The `fast` variant is eligible for a generated trampoline; `slow` exceeds the
// x86_64 register budget and falls back, so it resolves without allocating one.
// Comparing the two isolates trampoline creation cost from the rest of symbol
// resolution.

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

  // Warm up one-time initialization (libffi setup, executable memory probe) so
  // it is not attributed to the measured resolutions.
  lib.getFunction(name, definition);

  bench.start();
  for (let i = 0; i < n; ++i)
    lib.getFunction(name, definition);
  bench.end(n);

  lib.close();
}
