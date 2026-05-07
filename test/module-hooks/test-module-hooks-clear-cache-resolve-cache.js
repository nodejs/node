// Flags: --expose-internals
// Tests that clearCache clears the ESM resolve cache and re-evaluates the
// module on the next import.
'use strict';

const common = require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { clearCache } = require('node:module');
const { getOrInitializeCascadedLoader } = require('internal/modules/esm/loader');

const fixture = path.join(__dirname, '..', 'fixtures', 'module-cache', 'esm-counter.mjs');
const specifier = pathToFileURL(fixture).href;
const parentURL = pathToFileURL(__filename).href;

(async () => {
  const cascadedLoader = getOrInitializeCascadedLoader();

  const first = await import(specifier);
  assert.strictEqual(first.count, 1);
  assert.ok(cascadedLoader.hasResolveCacheEntry(specifier, parentURL),
            'resolve cache should have an entry after import');

  clearCache(specifier, {
    parentURL,
    resolver: 'import',
  });

  assert.ok(!cascadedLoader.hasResolveCacheEntry(specifier, parentURL),
            'resolve cache should be cleared by clearCache');

  const second = await import(specifier);
  assert.strictEqual(second.count, 2);

  delete globalThis.__module_cache_esm_counter;
})().then(common.mustCall());
