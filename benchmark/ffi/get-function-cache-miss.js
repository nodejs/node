'use strict';

// Measures the first getFunction() call on each library instance. This includes
// symbol resolution, signature preparation, and callable creation.
// The `fast` variant is eligible for a generated trampoline on supported
// platforms; `slow` exceeds the x86_64 register budget and falls back.
// Library construction and closing are outside the measured region.

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

  // Warm up one-time initialization without populating the measured caches.
  const warmup = new DynamicLibrary(libraryPath);
  warmup.getFunction(name, definition);
  warmup.close();

  // Each instance has its own cache, so its first lookup creates a callable.
  const libraries = Array.from({ length: n }, () => new DynamicLibrary(libraryPath));
  const functions = new Array(n);

  bench.start();
  for (let i = 0; i < n; ++i)
    functions[i] = libraries[i].getFunction(name, definition);
  bench.end(n);

  for (const lib of libraries)
    lib.close();
}
