// Tests that clearCache with importAttributes reloads a module imported
// with matching attributes, and that resolver: 'require' is a no-op that
// does not throw.
'use strict';

const common = require('../common');

const assert = require('node:assert');
const { pathToFileURL } = require('node:url');
const { clearCache, registerHooks } = require('node:module');

let loadCalls = 0;
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
      loadCalls++;
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
  assert.strictEqual(loadCalls, 1);

  // Matching importAttributes: the next import must load again.
  clearCache('virtual-json', {
    parentURL: pathToFileURL(__filename),
    resolver: 'import',
    importAttributes: { type: 'json' },
  });

  const second = await import('virtual-json', { with: { type: 'json' } });
  assert.deepStrictEqual(second.default, { key: 'value' });
  assert.strictEqual(loadCalls, 2);

  // resolver: 'require' should not throw even when importAttributes are set.
  clearCache('virtual-json', {
    parentURL: pathToFileURL(__filename),
    resolver: 'require',
    importAttributes: { type: 'json' },
  });

  hook.deregister();
})().then(common.mustCall());
