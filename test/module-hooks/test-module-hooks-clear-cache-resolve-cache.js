// Flags: --expose-internals
'use strict';

const common = require('../common');

const assert = require('node:assert');
const { pathToFileURL } = require('node:url');
const { clearCache, registerHooks } = require('node:module');
const { getOrInitializeCascadedLoader } = require('internal/modules/esm/loader');

let loadCalls = 0;
const hook = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'virtual') {
      return {
        url: 'virtual://cache-clear-resolve',
        format: 'module',
        shortCircuit: true,
      };
    }
    return nextResolve(specifier, context);
  },
  load(url, context, nextLoad) {
    if (url === 'virtual://cache-clear-resolve') {
      loadCalls++;
      return {
        format: 'module',
        source: 'export const count = ' +
          '(globalThis.__module_cache_virtual_counter ?? 0) + 1;\n' +
          'globalThis.__module_cache_virtual_counter = count;\n',
        shortCircuit: true,
      };
    }
    return nextLoad(url, context);
  },
});

(async () => {
  const first = await import('virtual');
  assert.strictEqual(first.count, 1);
  assert.strictEqual(loadCalls, 1);
  const loadCallsAfterFirst = loadCalls;

  const cascadedLoader = getOrInitializeCascadedLoader();
  let deleteResolveCalls = 0;
  const originalDeleteResolveCacheEntry = cascadedLoader.deleteResolveCacheEntry;
  cascadedLoader.deleteResolveCacheEntry = function(...args) {
    deleteResolveCalls++;
    return originalDeleteResolveCacheEntry.apply(this, args);
  };

  try {
    // caches: 'module' should NOT touch the resolve cache.
    clearCache('virtual', {
      parentURL: pathToFileURL(__filename),
      resolver: 'import',
      caches: 'module',
    });
    assert.strictEqual(deleteResolveCalls, 0);

    // caches: 'resolution' SHOULD clear the resolve cache entry.
    clearCache('virtual', {
      parentURL: pathToFileURL(__filename),
      resolver: 'import',
      caches: 'resolution',
    });
    assert.strictEqual(deleteResolveCalls, 1);

    // caches: 'all' SHOULD also clear the resolve cache entry.
    clearCache('virtual', {
      parentURL: pathToFileURL(__filename),
      resolver: 'import',
      caches: 'all',
    });
    assert.strictEqual(deleteResolveCalls, 2);
  } finally {
    cascadedLoader.deleteResolveCacheEntry = originalDeleteResolveCacheEntry;
  }

  const second = await import('virtual');
  assert.strictEqual(second.count, 2);
  assert.strictEqual(loadCalls, loadCallsAfterFirst + 1);

  hook.deregister();
  delete globalThis.__module_cache_virtual_counter;
})().then(common.mustCall());
