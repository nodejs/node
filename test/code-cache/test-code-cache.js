'use strict';

// Flags: --expose-internals
// This test verifies that if the binary is compiled with code cache,
// and the cache is used when built in modules are compiled.
// Otherwise, verifies that no cache is used when compiling builtins.

require('../common');
const assert = require('assert');
const {
  cachableBuiltins,
  cannotUseCache
} = require('internal/bootstrap/cache');
const {
  isMainThread
} = require('worker_threads');

const {
  internalBinding
} = require('internal/test/binding');
const {
  compiledWithoutCache,
  compiledWithCache
} = internalBinding('native_module');

for (const key of cachableBuiltins) {
  if (!isMainThread && key === 'trace_events') {
    continue;  // Cannot load trace_events in workers
  }
  require(key);
}

const loadedModules = process.moduleLoadList
  .filter((m) => m.startsWith('NativeModule'))
  .map((m) => m.replace('NativeModule ', ''));

// The binary is not configured with code cache, verifies that the builtins
// are all compiled without cache and we are doing the bookkeeping right.
if (process.config.variables.node_code_cache_path === undefined) {
  console.log('The binary is not configured with code cache');
  assert.deepStrictEqual(compiledWithCache, new Set());
  assert.deepStrictEqual(compiledWithoutCache, new Set(loadedModules));
} else {
  console.log('The binary is configured with code cache');
  assert.strictEqual(
    typeof process.config.variables.node_code_cache_path,
    'string'
  );

  for (const key of loadedModules) {
    if (cannotUseCache.includes(key)) {
      assert(compiledWithoutCache.has(key),
             `"${key}" should've been compiled without code cache`);
    } else {
      assert(compiledWithCache.has(key),
             `"${key}" should've been compiled with code cache`);
    }
  }
}
