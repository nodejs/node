// Flags: --expose-internals
'use strict';

const common = require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { clearCache } = require('node:module');
const { clearCjsCache } = require('internal/modules/esm/translators');

const fixturePath = path.join(__dirname, '..', 'fixtures', 'module-cache', 'cjs-counter.js');
const url = pathToFileURL(fixturePath);

(async () => {
  const first = await import(`${url.href}?v=1`);
  assert.strictEqual(first.default.count, 1);

  const result = clearCache(url);
  assert.strictEqual(result.require, true);
  assert.strictEqual(result.import, true);

  assert.strictEqual(clearCjsCache(`${url.href}?v=1`), false);
  delete globalThis.__module_cache_cjs_counter;
})().then(common.mustCall());
