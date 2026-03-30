'use strict';

// Test that the context parameter will be merged in multiple load hooks.

const common = require('../common');
const assert = require('assert');
const { registerHooks } = require('module');

const hook1 = registerHooks({
  load: common.mustCall(function(url, context, nextLoad) {
    assert.strictEqual(context.testProp, 'custom');  // It should be merged from hook 2 and 3.
    return nextLoad(url, context);
  }, 1),
});

const hook2 = registerHooks({
  load: common.mustCall(function(url, context, nextLoad) {
    assert.strictEqual(context.testProp, 'custom');  // It should be merged from hook 3.
    return nextLoad(url);  // Omit the context.
  }, 1),
});

const hook3 = registerHooks({
  load: common.mustCall(function(url, context, nextLoad) {
    return nextLoad(url, { testProp: 'custom' });  // Add a custom property
  }, 1),
});

require('../fixtures/empty.js');

hook3.deregister();
hook2.deregister();
hook1.deregister();
