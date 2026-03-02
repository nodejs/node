'use strict';

// This tests that require.resolve() and require() both go through the same
// resolve hooks registered via module.registerHooks().

require('../common');
const assert = require('assert');
const { registerHooks } = require('module');
const fixtures = require('../common/fixtures');
const { pathToFileURL } = require('url');

const redirectedPath = fixtures.path('module-hooks', 'redirected-assert.js');

const resolvedSpecifiers = [];
const hook = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'test-consistency-target') {
      resolvedSpecifiers.push(specifier);
      return {
        url: pathToFileURL(redirectedPath).href,
        shortCircuit: true,
      };
    }
    return nextResolve(specifier, context);
  },
});

const resolveResult = require.resolve('test-consistency-target');
const requireResult = require('test-consistency-target');

assert.strictEqual(resolveResult, redirectedPath);
assert.strictEqual(requireResult.exports_for_test, 'redirected assert');
assert.deepStrictEqual(resolvedSpecifiers, [
  'test-consistency-target',
  'test-consistency-target',
]);

hook.deregister();
