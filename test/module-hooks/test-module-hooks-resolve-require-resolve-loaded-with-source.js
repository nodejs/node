'use strict';

// This tests that require.resolve() in a CJS file loaded via the re-invented
// require (triggered when module.register() installs an async loader that
// provides source for a CJS file) still goes through resolve hooks registered
// via module.registerHooks().

const common = require('../common');
const assert = require('assert');
const { register, registerHooks } = require('module');
const fixtures = require('../common/fixtures');
const { pathToFileURL } = require('url');

const redirectedPath = fixtures.path('module-hooks', 'redirected-assert.js');

// Register an async loader that provides source for CJS files, which triggers
// the re-invented require path.
register(fixtures.fileURL('module-hooks', 'logger-async-hooks.mjs'));

const hook = registerHooks({
  resolve: common.mustCall((specifier, context, nextResolve) => {
    if (specifier === 'test-require-resolve-hook-target') {
      return {
        url: pathToFileURL(redirectedPath).href,
        shortCircuit: true,
      };
    }
    return nextResolve(specifier, context);
  }, 2),
});

import('../fixtures/module-hooks/require-resolve-caller.js').then(common.mustCall((ns) => {
  assert.strictEqual(ns.default.resolved, redirectedPath);
  hook.deregister();
}));
