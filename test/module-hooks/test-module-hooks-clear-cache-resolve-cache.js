// Tests that clearCache causes the next import to re-evaluate the module
// when customization hooks are registered.
'use strict';

const common = require('../common');

const assert = require('node:assert');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { clearCache, registerHooks } = require('node:module');

const fixture = path.join(__dirname, '..', 'fixtures', 'module-cache', 'esm-counter.mjs');
const specifier = pathToFileURL(fixture).href;
const parentURL = pathToFileURL(__filename).href;

let loadCalls = 0;
const hook = registerHooks({
  load(url, context, nextLoad) {
    if (url.split('?')[0] === specifier) {
      loadCalls++;
    }
    return nextLoad(url, context);
  },
});

(async () => {
  const first = await import(specifier);
  assert.strictEqual(first.count, 1);
  assert.strictEqual(loadCalls, 1);

  clearCache(specifier, {
    parentURL,
    resolver: 'import',
  });

  const second = await import(specifier);
  assert.strictEqual(second.count, 2);
  assert.strictEqual(loadCalls, 2);

  hook.deregister();
  delete globalThis.__module_cache_esm_counter;
})().then(common.mustCall());
