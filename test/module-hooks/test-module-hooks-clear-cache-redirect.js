'use strict';

// Tests that clearCache with resolver: 'import' respects registered hooks
// that redirect specifiers. When a hook redirects specifier A to specifier B,
// clearCache(A) should clear the cache for B (the redirected target).

const common = require('../common');

const assert = require('node:assert');
const { pathToFileURL } = require('node:url');
const { clearCache, registerHooks } = require('node:module');

const hook = registerHooks({
  resolve(specifier, context, nextResolve) {
    // Redirect 'redirected-esm' to a virtual URL.
    if (specifier === 'redirected-esm') {
      return {
        url: 'virtual://redirected-target',
        format: 'module',
        shortCircuit: true,
      };
    }
    return nextResolve(specifier, context);
  },
  load(url, context, nextLoad) {
    if (url === 'virtual://redirected-target') {
      return {
        format: 'module',
        source: 'globalThis.__module_cache_redirect_counter = ' +
          '(globalThis.__module_cache_redirect_counter ?? 0) + 1;\n' +
          'export const count = globalThis.__module_cache_redirect_counter;\n',
        shortCircuit: true,
      };
    }
    return nextLoad(url, context);
  },
});

(async () => {
  const first = await import('redirected-esm');
  assert.strictEqual(first.count, 1);

  // Clear using the original specifier — hooks should resolve it
  // to the redirected target and clear that cache.
  clearCache('redirected-esm', {
    parentURL: pathToFileURL(__filename),
    resolver: 'import',
    caches: 'all',
  });

  const second = await import('redirected-esm');
  assert.strictEqual(second.count, 2);

  hook.deregister();
  delete globalThis.__module_cache_redirect_counter;
})().then(common.mustCall());
