'use strict';

require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { clearCache } = require('node:module');

const fixture = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-counter.js');
const relativeSpecifier = '../fixtures/module-cache/cjs-counter.js';

require(fixture);

assert.throws(() => clearCache(fixture, { mode: 'cjs' }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => clearCache(fixture, { mode: 'esm' }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => clearCache(relativeSpecifier, { parentURL: __filename }), {
  code: 'ERR_INVALID_ARG_VALUE',
});

const commonjsResult = clearCache(relativeSpecifier, {
  mode: 'commonjs',
  parentURL: pathToFileURL(__filename),
});
assert.strictEqual(commonjsResult.commonjs, true);

const first = require(fixture);
const moduleResult = clearCache(fixture, { mode: 'module' });
assert.strictEqual(moduleResult.commonjs, false);
assert.strictEqual(moduleResult.module, false);
const second = require(fixture);
assert.strictEqual(first, second);

delete globalThis.__module_cache_cjs_counter;
