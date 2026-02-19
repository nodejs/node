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
  const originalDeleteResolveCacheByFilename = cascadedLoader.deleteResolveCacheByFilename;
  cascadedLoader.deleteResolveCacheEntry = function(...args) {
    deleteResolveCalls++;
    return originalDeleteResolveCacheEntry.apply(this, args);
  };
  cascadedLoader.deleteResolveCacheByFilename = function(...args) {
    deleteResolveCalls++;
    return originalDeleteResolveCacheByFilename.apply(this, args);
  };

  try {
    const result = clearCache('virtual', {
      parentURL: pathToFileURL(__filename),
    });
    assert.strictEqual(result.require, false);
    assert.strictEqual(result.import, true);
    assert.strictEqual(deleteResolveCalls, 0);
  } finally {
    cascadedLoader.deleteResolveCacheEntry = originalDeleteResolveCacheEntry;
    cascadedLoader.deleteResolveCacheByFilename = originalDeleteResolveCacheByFilename;
  }

  const second = await import('virtual');
  assert.strictEqual(second.count, 2);
  assert.strictEqual(loadCalls, loadCallsAfterFirst + 1);

  hook.deregister();
  delete globalThis.__module_cache_virtual_counter;
})().then(common.mustCall());
