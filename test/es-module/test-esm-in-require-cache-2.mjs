// This tests the behavior of ESM in require.cache when it's loaded from import.

import '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';
const filename = fixtures.path('es-modules', 'esm-in-require-cache', 'esm.mjs');
import { Module } from 'node:module';

// Imported ESM should not be in the require cache.
let { name } = await import('../fixtures/es-modules/esm-in-require-cache/import-esm.mjs');
assert.strictEqual(name, 'esm');
assert(!Module._cache[filename]);

({ name } = await import('../fixtures/es-modules/esm-in-require-cache/esm.mjs'));
assert.strictEqual(name, 'esm');
assert(!Module._cache[filename]);

// Requiring ESM indirectly should not put it in the cache.
({ name } = await import('../fixtures/es-modules/esm-in-require-cache/require-import-esm.cjs'));
assert.strictEqual(name, 'esm');
assert(!Module._cache[filename]);

// After being required directly, it should be in the cache.
({ name } = await import('../fixtures/es-modules/esm-in-require-cache/import-require-esm.mjs'));
assert.strictEqual(name, 'esm');
assert(Module._cache[filename]);
