'use strict';

// This tests that require.resolve() works with builtin redirection
// via resolve hooks registered with module.registerHooks().

require('../common');
const assert = require('assert');
const { registerHooks } = require('module');

const hook = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'assert') {
      return {
        url: 'node:zlib',
        shortCircuit: true,
      };
    }
    return nextResolve(specifier, context);
  },
});

const resolved = require.resolve('assert');
assert.strictEqual(resolved, 'zlib');

hook.deregister();
