'use strict';

require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { clearCache } = require('node:module');

const fixture = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-counter.js');

const first = require(fixture);
const second = require(fixture);

assert.strictEqual(first.count, 1);
assert.strictEqual(second.count, 1);
assert.strictEqual(first, second);

const result = clearCache(fixture);
assert.strictEqual(result.cjs, true);
assert.strictEqual(result.esm, false);

const third = require(fixture);
assert.strictEqual(third.count, 2);

delete globalThis.__module_cache_cjs_counter;
