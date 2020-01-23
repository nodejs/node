// Flags: --expose-internals
'use strict';

// This test verifies that if the binary is compiled with code cache,
// and the cache is used when built in modules are compiled.
// Otherwise, verifies that no cache is used when compiling builtins.

const { isMainThread } = require('../common');
const assert = require('assert');
const {
  internalBinding
} = require('internal/test/binding');
const {
  getCacheUsage,
  moduleCategories: { canBeRequired, cannotBeRequired }
} = internalBinding('native_module');

for (const key of canBeRequired) {
  require(key);
}

// The computation has to be delayed until we have done loading modules
const {
  compiledWithoutCache,
  compiledWithCache
} = getCacheUsage();

const loadedModules = process.moduleLoadList
  .filter((m) => m.startsWith('NativeModule'))
  .map((m) => m.replace('NativeModule ', ''));

// Cross-compiled binaries do not have code cache, verifies that the builtins
// are all compiled without cache and we are doing the bookkeeping right.
if (!process.features.cached_builtins) {
  console.log('The binary is not configured with code cache');
  assert(!process.config.variables.node_use_node_code_cache);

  if (isMainThread) {
    assert.deepStrictEqual(compiledWithCache, new Set());
    for (const key of loadedModules) {
      assert(compiledWithoutCache.has(key),
             `"${key}" should've been compiled without code cache`);
    }
  } else {
    // TODO(joyeecheung): create a list of modules whose cache can be shared
    // from the main thread to the worker thread and check that their
    // cache are hit
    assert.notDeepStrictEqual(compiledWithCache, new Set());
  }
} else {  // Native compiled
  assert(process.config.variables.node_use_node_code_cache);

  if (!isMainThread) {
    for (const key of [ 'internal/bootstrap/pre_execution' ]) {
      canBeRequired.add(key);
      cannotBeRequired.delete(key);
    }
  }

  const wrong = [];
  for (const key of loadedModules) {
    if (cannotBeRequired.has(key) && !compiledWithoutCache.has(key)) {
      wrong.push(`"${key}" should've been compiled **without** code cache`);
    }
    if (canBeRequired.has(key) && !compiledWithCache.has(key)) {
      wrong.push(`"${key}" should've been compiled **with** code cache`);
    }
  }
  assert.strictEqual(wrong.length, 0, wrong.join('\n'));
}
