'use strict';

// Flags: --expose-internals
// This test verifies that if the binary is compiled with code cache,
// and the cache is used when built in modules are compiled.
// Otherwise, verifies that no cache is used when compiling builtins.

require('../common');
const assert = require('assert');
const {
  types: {
    isUint8Array
  }
} = require('util');
const {
  cachableBuiltins,
  codeCache,
  compiledWithCache,
  compiledWithoutCache
} = require('internal/bootstrap/cache');

const loadedModules = process.moduleLoadList
  .filter((m) => m.startsWith('NativeModule'))
  .map((m) => m.replace('NativeModule ', ''));

// The binary is not configured with code cache, verifies that the builtins
// are all compiled without cache and we are doing the bookkeeping right.
if (process.config.variables.node_code_cache_path === undefined) {
  assert.deepStrictEqual(compiledWithCache, []);
  assert.notStrictEqual(compiledWithoutCache.length, 0);

  for (const key of loadedModules) {
    assert(compiledWithoutCache.includes(key),
           `"${key}" should not have been compiled with code cache`);
  }

} else {
  // The binary is configured with code cache.
  assert.strictEqual(
    typeof process.config.variables.node_code_cache_path,
    'string'
  );
  assert.deepStrictEqual(compiledWithoutCache, []);

  for (const key of loadedModules) {
    assert(compiledWithCache.includes(key),
           `"${key}" should've been compiled with code cache`);
  }

  for (const key of cachableBuiltins) {
    assert(isUint8Array(codeCache[key]) && codeCache[key].length > 0,
           `Code cache for "${key}" should've been generated`);
  }
}
