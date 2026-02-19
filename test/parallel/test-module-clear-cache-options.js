'use strict';

require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { clearCache } = require('node:module');

const fixture = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-counter.js');
const relativeSpecifier = '../fixtures/module-cache/cjs-counter.js';

require(fixture);

assert.throws(() => clearCache(relativeSpecifier, { parentURL: __filename }), {
  code: 'ERR_INVALID_ARG_VALUE',
});

const requireResult = clearCache(relativeSpecifier, {
  parentURL: pathToFileURL(__filename),
});
assert.strictEqual(requireResult.require, true);

const first = require(fixture);
const importResult = clearCache(fixture);
assert.strictEqual(importResult.require, true);
assert.strictEqual(importResult.import, false);
const second = require(fixture);
assert.notStrictEqual(first.count, second.count);
assert.strictEqual(first.count, 2);
assert.strictEqual(second.count, 3);

delete globalThis.__module_cache_cjs_counter;
