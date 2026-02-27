'use strict';

// This tests that require.resolve() invokes resolve hooks registered
// via module.registerHooks() and can redirect to a different file.

require('../common');
const assert = require('assert');
const { registerHooks } = require('module');
const fixtures = require('../common/fixtures');

const redirectedPath = fixtures.path('module-hooks', 'redirected-assert.js');

const hook = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'test-resolve-target') {
      return {
        url: `file://${redirectedPath}`,
        shortCircuit: true,
      };
    }
    return nextResolve(specifier, context);
  },
});

const resolved = require.resolve('test-resolve-target');
assert.strictEqual(resolved, redirectedPath);

hook.deregister();
