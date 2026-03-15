'use strict';

// This tests that require.resolve() falls through to default resolution
// when resolve hooks registered via module.registerHooks() don't override.

require('../common');
const assert = require('assert');
const { registerHooks } = require('module');

const hook = registerHooks({
  resolve(specifier, context, nextResolve) {
    return nextResolve(specifier, context);
  },
});

const resolved = require.resolve('assert');
assert.strictEqual(resolved, 'assert');

hook.deregister();
