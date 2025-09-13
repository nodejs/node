// This tests the behavior of ESM in require.cache when it's loaded from require.
'use strict';
require('../common');
const assert = require('node:assert');
const fixtures = require('../common/fixtures');
const filename = fixtures.path('es-modules', 'esm-in-require-cache', 'esm.mjs');

// Requiring ESM indirectly should not put it in the cache.
let { name } = require('../fixtures/es-modules/esm-in-require-cache/import-esm.mjs');
assert.strictEqual(name, 'esm');
assert(!require.cache[filename]);

({ name } = require('../fixtures/es-modules/esm-in-require-cache/require-import-esm.cjs'));
assert.strictEqual(name, 'esm');
assert(!require.cache[filename]);

// After being required directly, it should be in the cache.
({ name } = require('../fixtures/es-modules/esm-in-require-cache/esm.mjs'));
assert.strictEqual(name, 'esm');
assert(require.cache[filename]);
delete require.cache[filename];

({ name } = require('../fixtures/es-modules/esm-in-require-cache/import-require-esm.mjs'));
assert.strictEqual(name, 'esm');
assert(require.cache[filename]);
