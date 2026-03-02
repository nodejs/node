// Flags: --expose-internals
// Tests that caches: 'module' does NOT clear the resolve cache,
// while caches: 'resolution' and caches: 'all' DO clear it.
// Uses the exposed hasResolveCacheEntry method instead of monkey-patching.
'use strict';

const common = require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { clearCache } = require('node:module');
const { getOrInitializeCascadedLoader } = require('internal/modules/esm/loader');

// Use a real file-based specifier so the resolve cache is populated
// by the default resolver (the resolve cache is used for non-hook paths).
const fixture = path.join(__dirname, '..', 'fixtures', 'module-cache', 'esm-counter.mjs');
const specifier = pathToFileURL(fixture).href;
const parentURL = pathToFileURL(__filename).href;

(async () => {
  const cascadedLoader = getOrInitializeCascadedLoader();

  // --- Test 1: caches: 'module' should NOT clear the resolve cache ---
  const first = await import(specifier);
  assert.strictEqual(first.count, 1);

  // After import, the resolve cache should have an entry.
  assert.ok(cascadedLoader.hasResolveCacheEntry(specifier, parentURL),
            'resolve cache should have an entry after import');

  // caches: 'module' should NOT clear the resolve cache entry.
  clearCache(specifier, {
    parentURL,
    resolver: 'import',
    caches: 'module',
  });
  assert.ok(cascadedLoader.hasResolveCacheEntry(specifier, parentURL),
            'resolve cache should still have entry after caches: "module"');

  // Re-import to repopulate the load cache (since 'module' cleared it).
  const afterModuleClear = await import(specifier);
  assert.strictEqual(afterModuleClear.count, 2);

  // --- Test 2: caches: 'resolution' SHOULD clear the resolve cache,
  //             but NOT re-evaluate the module (load cache still holds it) ---
  clearCache(specifier, {
    parentURL,
    resolver: 'import',
    caches: 'resolution',
  });
  assert.ok(!cascadedLoader.hasResolveCacheEntry(specifier, parentURL),
            'resolve cache should be cleared after caches: "resolution"');

  // Re-import: module should NOT be re-evaluated — load cache still holds it.
  const afterResClear = await import(specifier);
  assert.strictEqual(afterResClear.count, 2);

  // --- Test 3: caches: 'all' SHOULD clear both resolve and load caches ---
  // Repopulate the resolve cache first.
  await import(specifier);

  clearCache(specifier, {
    parentURL,
    resolver: 'import',
    caches: 'all',
  });
  assert.ok(!cascadedLoader.hasResolveCacheEntry(specifier, parentURL),
            'resolve cache should be cleared after caches: "all"');

  // After 'all', re-import should re-evaluate.
  const afterAllClear = await import(specifier);
  assert.strictEqual(afterAllClear.count, 3);

  delete globalThis.__module_cache_esm_counter;
})().then(common.mustCall());
