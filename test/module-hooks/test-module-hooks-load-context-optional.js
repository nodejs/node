'use strict';

// Test that the context parameter can be omitted in the nextLoad invocation.

const common = require('../common');
const { registerHooks } = require('module');

const hook = registerHooks({
  load: common.mustCall(function(url, context, nextLoad) {
    return nextLoad(url);
  }, 1),
});

require('../fixtures/empty.js');

hook.deregister();
