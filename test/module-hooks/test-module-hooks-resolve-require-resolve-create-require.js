'use strict';

// This tests that require.resolve() from a require function returned by
// module.createRequire() goes through resolve hooks registered via
// module.registerHooks().

require('../common');
const assert = require('assert');
const { registerHooks, createRequire } = require('module');
const fixtures = require('../common/fixtures');

const redirectedPath = fixtures.path('module-hooks', 'redirected-assert.js');

const hook = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'test-create-require-resolve-target') {
      return {
        url: `file://${redirectedPath}`,
        shortCircuit: true,
      };
    }
    return nextResolve(specifier, context);
  },
});

const customRequire = createRequire(__filename);
const resolved = customRequire.resolve('test-create-require-resolve-target');
assert.strictEqual(resolved, redirectedPath);

hook.deregister();
