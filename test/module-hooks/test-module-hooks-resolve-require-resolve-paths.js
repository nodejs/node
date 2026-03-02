'use strict';

// This tests that require.resolve() with the paths option work transparently
// when resolve hooks are registered via module.registerHooks().

require('../common');
const assert = require('assert');
const path = require('path');
const { registerHooks } = require('module');
const fixtures = require('../common/fixtures');

const nodeModules = path.join(fixtures.path(), 'node_modules');
const resolveCallCount = [];

const hook = registerHooks({
  resolve(specifier, context, nextResolve) {
    if (specifier === 'bar') {
      resolveCallCount.push(specifier);
    }
    return nextResolve(specifier, context);
  },
});

// require.resolve with paths option should go through hooks and resolve correctly.
const resolved = require.resolve('bar', { paths: [fixtures.path()] });
assert.strictEqual(resolved, path.join(nodeModules, 'bar.js'));
assert.deepStrictEqual(resolveCallCount, ['bar']);

hook.deregister();
