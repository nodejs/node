// Tests that clearCache clears caches across BOTH CJS and ESM,
// regardless of which resolver is used.
//
// - resolver 'require' resolves via CJS → also clears ESM load cache
// - resolver 'import'  resolves via ESM → also clears CJS Module._cache

import '../common/index.mjs';

import assert from 'node:assert';
import { clearCache, createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const fixtureURL = new URL('../fixtures/module-cache/cjs-counter.js', import.meta.url);
const fixturePath = fileURLToPath(fixtureURL);

// --- Test 1: resolver 'require' also clears ESM load cache ---

// Load via require.
const cjs1 = require(fixturePath);
assert.strictEqual(cjs1.count, 1);

// Load via import — uses Module._cache entry, same count.
const esm1 = await import(fixtureURL.href);
assert.strictEqual(esm1.default.count, 1);

// Clear with resolver: 'require'. This resolves to the filename, converts
// to file URL, then clears Module._cache AND ESM load cache.
clearCache(fixturePath, {
  parentURL: import.meta.url,
  resolver: 'require',
});

// CJS cache should be cleared.
assert.strictEqual(require.cache[fixturePath], undefined);

// ESM load cache should also be cleared — re-import must re-evaluate.
const esm2 = await import(fixtureURL.href);
assert.strictEqual(esm2.default.count, 2);

// --- Test 2: resolver 'import' also clears CJS Module._cache ---

// Ensure CJS cache is populated (import above already did this via translator).
assert.notStrictEqual(require.cache[fixturePath], undefined);

// Clear with resolver: 'import'. This resolves to a file URL, extracts
// the file path, then clears both ESM and CJS caches.
clearCache(fixtureURL, {
  parentURL: import.meta.url,
  resolver: 'import',
});

// CJS cache must be cleared even though we used the 'import' resolver.
assert.strictEqual(require.cache[fixturePath], undefined);

// Re-require must re-evaluate.
const cjs3 = require(fixturePath);
assert.strictEqual(cjs3.count, 3);

delete globalThis.__module_cache_cjs_counter;
