'use strict';
// Test that load hook in imported CJS only gets invoked once.

const common = require('../common');
const assert = require('assert');
const { registerHooks } = require('module');
const path = require('path');
const { pathToFileURL } = require('url');

const hook = registerHooks({
  load: common.mustCall(function(url, context, nextLoad) {
    assert.strictEqual(url, pathToFileURL(path.resolve(__dirname, '../fixtures/value.cjs')).href);
    return nextLoad(url, context);
  }, 1),
});

import('../fixtures/value.cjs').then(common.mustCall((result) => {
  assert.strictEqual(result.value, 42);
  hook.deregister();
}));
