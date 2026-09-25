'use strict';
// Test that resolve hook in imported CJS only gets invoked once.

const common = require('../common');
const assert = require('assert');
const { registerHooks } = require('module');

const hook = registerHooks({
  resolve: common.mustCall(function(specifier, context, nextResolve) {
    assert.strictEqual(specifier, '../fixtures/value.cjs');
    return nextResolve(specifier, context);
  }, 1),
});

import('../fixtures/value.cjs').then(common.mustCall((result) => {
  assert.strictEqual(result.value, 42);
  hook.deregister();
}));
