// Tests that caches: 'resolution' only clears the ESM resolution cache.
// The module itself is NOT re-evaluated because the load cache still holds it.
// Also tests that caches: 'resolution' with resolver: 'require' is harmless
// (CJS has no separate resolution cache).

import '../common/index.mjs';

import assert from 'node:assert';
import { clearCache, createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);

// --- ESM: resolution-only clearing should NOT re-evaluate ---

const specifier = '../fixtures/module-cache/esm-counter.mjs';

const first = await import(specifier);
assert.strictEqual(first.count, 1);

// Clear only resolution cache.
clearCache(specifier, {
  parentURL: import.meta.url,
  resolver: 'import',
  caches: 'resolution',
});

// Module must NOT be re-evaluated — load cache still holds it.
const second = await import(specifier);
assert.strictEqual(second.count, 1);
assert.strictEqual(first, second);

// Now clear the module cache — module SHOULD be re-evaluated.
clearCache(specifier, {
  parentURL: import.meta.url,
  resolver: 'import',
  caches: 'module',
});

const third = await import(specifier);
assert.strictEqual(third.count, 2);

// --- CJS: resolution-only clearing should be a no-op ---

const cjsFixturePath = fileURLToPath(
  new URL('../fixtures/module-cache/cjs-counter.js', import.meta.url),
);

require(cjsFixturePath);
assert.notStrictEqual(require.cache[cjsFixturePath], undefined);

// caches: 'resolution' + resolver: 'require' → no-op for CJS.
clearCache(cjsFixturePath, {
  parentURL: import.meta.url,
  resolver: 'require',
  caches: 'resolution',
});

// Module._cache should be unaffected.
assert.notStrictEqual(require.cache[cjsFixturePath], undefined);

delete globalThis.__module_cache_esm_counter;
delete globalThis.__module_cache_cjs_counter;
