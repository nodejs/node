// Flags: --expose-internals
// Tests that clearCache clears the CommonJS module cache and the related
// resolution caches for the resolved filename.
'use strict';

require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const Module = require('node:module');
const { clearCache } = require('node:module');

const fixture = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-counter.js');

require(fixture);
assert.notStrictEqual(Module._cache[fixture], undefined);

const hasPathCacheEntry = () =>
  Object.keys(Module._pathCache).some((key) => Module._pathCache[key] === fixture);

assert.ok(hasPathCacheEntry(), 'Module._pathCache should contain the fixture');

clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
});

assert.ok(!hasPathCacheEntry(),
          'Module._pathCache should not contain the fixture after clearCache');
assert.strictEqual(Module._cache[fixture], undefined);

const reloaded = require(fixture);
assert.strictEqual(reloaded.count, 2);

delete globalThis.__module_cache_cjs_counter;
