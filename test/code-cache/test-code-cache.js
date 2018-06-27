'use strict';

// Flags: --expose-internals
// This test verifies that the binary is compiled with code cache and the
// cache is used when built in modules are compiled.

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

assert.strictEqual(
  typeof process.config.variables.node_code_cache_path,
  'string'
);

assert.deepStrictEqual(compiledWithoutCache, []);

const loadedModules = process.moduleLoadList
  .filter((m) => m.startsWith('NativeModule'))
  .map((m) => m.replace('NativeModule ', ''));

for (const key of loadedModules) {
  assert(compiledWithCache.includes(key),
         `"${key}" should've been compiled with code cache`);
}

for (const key of cachableBuiltins) {
  assert(isUint8Array(codeCache[key]) && codeCache[key].length > 0,
         `Code cache for "${key}" should've been generated`);
}
