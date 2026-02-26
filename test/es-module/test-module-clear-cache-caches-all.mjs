// Tests that caches: 'all' clears both the resolution cache
// and the module cache. The module IS re-evaluated after clearing.

import '../common/index.mjs';

import assert from 'node:assert';
import { clearCache } from 'node:module';

const specifier = '../fixtures/module-cache/esm-counter.mjs';

const first = await import(specifier);
assert.strictEqual(first.count, 1);

// caches: 'all' — clears both resolution and module caches.
clearCache(specifier, {
  parentURL: import.meta.url,
  resolver: 'import',
  caches: 'all',
});

// Module should be re-evaluated.
const second = await import(specifier);
assert.strictEqual(second.count, 2);

// Clearing again with 'all' should also work.
clearCache(specifier, {
  parentURL: import.meta.url,
  resolver: 'import',
  caches: 'all',
});

const third = await import(specifier);
assert.strictEqual(third.count, 3);

delete globalThis.__module_cache_esm_counter;
