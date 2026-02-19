'use strict';

require('../common');

const assert = require('node:assert');
const { pathToFileURL } = require('node:url');
const { clearCache, registerHooks } = require('node:module');

let loadCalls = 0;
const hook = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'virtual') {
      return {
        url: 'virtual://cache-clear',
        format: 'commonjs',
        shortCircuit: true,
      };
    }
    return nextResolve(specifier, context);
  },
  load(url, context, nextLoad) {
    if (url === 'virtual://cache-clear') {
      loadCalls++;
      return {
        format: 'commonjs',
        source: 'globalThis.__module_cache_virtual_counter = ' +
          '(globalThis.__module_cache_virtual_counter ?? 0) + 1;' +
          'module.exports = { count: globalThis.__module_cache_virtual_counter };',
        shortCircuit: true,
      };
    }
    return nextLoad(url, context);
  },
});

const first = require('virtual');
assert.strictEqual(first.count, 1);

const result = clearCache('virtual', {
  parentURL: pathToFileURL(__filename),
});
assert.strictEqual(result.require, true);
assert.strictEqual(result.import, false);

const second = require('virtual');
assert.strictEqual(second.count, 2);
assert.strictEqual(loadCalls, 2);

hook.deregister();
delete globalThis.__module_cache_virtual_counter;
