'use strict';

// Test that the context parameter can be omitted in the nextResolve invocation.

const common = require('../common');
const { registerHooks } = require('module');

const hook = registerHooks({
  resolve: common.mustCall(function(specifier, context, nextResolve) {
    return nextResolve(specifier);
  }, 1),
});

require('../fixtures/empty.js');

hook.deregister();
