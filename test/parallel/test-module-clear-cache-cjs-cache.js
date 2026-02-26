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

  clearCache(url, {
    parentURL: pathToFileURL(__filename),
    resolver: 'import',
    caches: 'module',
  });

  // Verify the cjsCache was also cleared for query variants.
  assert.strictEqual(clearCjsCache(`${url.href}?v=1`), false);
  delete globalThis.__module_cache_cjs_counter;
})().then(common.mustCall());
