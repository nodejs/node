// Flags: --expose-internals
// Tests that the importAttributes option is forwarded correctly
// to the ESM resolve cache deletion.
'use strict';

const common = require('../common');

const assert = require('node:assert');
const { pathToFileURL } = require('node:url');
const { clearCache, registerHooks } = require('node:module');
const { getOrInitializeCascadedLoader } = require('internal/modules/esm/loader');

const hook = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'virtual-json') {
      return {
        url: 'virtual://json-data',
        format: 'json',
        shortCircuit: true,
      };
    }
    return nextResolve(specifier, context);
  },
  load(url, context, nextLoad) {
    if (url === 'virtual://json-data') {
      return {
        format: 'json',
        source: '{"key": "value"}',
        shortCircuit: true,
      };
    }
    return nextLoad(url, context);
  },
});

(async () => {
  const first = await import('virtual-json', { with: { type: 'json' } });
  assert.deepStrictEqual(first.default, { key: 'value' });

  const cascadedLoader = getOrInitializeCascadedLoader();
  const capturedCalls = [];
  const original = cascadedLoader.deleteResolveCacheEntry;
  cascadedLoader.deleteResolveCacheEntry = function(specifier, parentURL, importAttributes) {
    capturedCalls.push({ specifier, parentURL, importAttributes });
    return original.call(this, specifier, parentURL, importAttributes);
  };

  try {
    // Without importAttributes — default empty object is forwarded.
    clearCache('virtual-json', {
      parentURL: pathToFileURL(__filename),
      resolver: 'import',
    });
    assert.strictEqual(capturedCalls.length, 1);
    assert.strictEqual(capturedCalls[0].specifier, 'virtual-json');
    assert.deepStrictEqual(Object.keys(capturedCalls[0].importAttributes), []);

    // With importAttributes: { type: 'json' } — forwarded through.
    clearCache('virtual-json', {
      parentURL: pathToFileURL(__filename),
      resolver: 'import',
      importAttributes: { type: 'json' },
    });
    assert.strictEqual(capturedCalls.length, 2);
    assert.deepStrictEqual(capturedCalls[1].importAttributes, { type: 'json' });

    // resolver: 'require' should NOT call deleteResolveCacheEntry
    // even though clearCache still clears the CommonJS-side caches.
    clearCache('virtual-json', {
      parentURL: pathToFileURL(__filename),
      resolver: 'require',
      importAttributes: { type: 'json' },
    });
    assert.strictEqual(capturedCalls.length, 2);
  } finally {
    cascadedLoader.deleteResolveCacheEntry = original;
  }

  hook.deregister();
})().then(common.mustCall());
