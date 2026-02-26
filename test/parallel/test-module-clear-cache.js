'use strict';

require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { clearCache } = require('node:module');

const fixture = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-counter.js');

const first = require(fixture);
const second = require(fixture);

assert.strictEqual(first.count, 1);
assert.strictEqual(second.count, 1);
assert.strictEqual(first, second);

clearCache(fixture, {
  parentURL: pathToFileURL(__filename),
  resolver: 'require',
  caches: 'module',
});

assert.strictEqual(require.cache[fixture], undefined);

const third = require(fixture);
assert.strictEqual(third.count, 2);

delete globalThis.__module_cache_cjs_counter;
