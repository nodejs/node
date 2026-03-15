// Flags: --expose-internals
// Tests that clearCache with caches: 'resolution' or 'all' also clears
// the CJS relativeResolveCache and Module._pathCache entries.
'use strict';

require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const Module = require('node:module');
const { clearCache } = require('node:module');

const fixture = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-counter.js');

// Load via require to populate relativeResolveCache and _pathCache.
require(fixture);
assert.notStrictEqual(Module._cache[fixture], undefined);

// Module._pathCache should have an entry pointing to this fixture.
const pathCacheKeys = Object.keys(Module._pathCache);
const hasPathCacheEntry = pathCacheKeys.some(
  (key) => Module._pathCache[key] === fixture,
);
assert.ok(hasPathCacheEntry, 'Module._pathCache should contain the fixture');

// Clear only resolution caches.
clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
  caches: 'resolution',
});

// Module._cache should still be present (we only cleared resolution).
assert.notStrictEqual(Module._cache[fixture], undefined);

// But _pathCache entries for this filename should be cleared.
const pathCacheKeysAfter = Object.keys(Module._pathCache);
const hasPathCacheEntryAfter = pathCacheKeysAfter.some(
  (key) => Module._pathCache[key] === fixture,
);
assert.ok(!hasPathCacheEntryAfter,
          'Module._pathCache should not contain the fixture after clearing resolution');

delete globalThis.__module_cache_cjs_counter;
