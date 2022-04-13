'use strict';
const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

const hook = async_hooks.createHook({
  init: common.mustCall(() => {}, 1),
  before: common.mustNotCall(),
  after: common.mustNotCall(),
  destroy: common.mustNotCall()
});

assert.strictEqual(hook.enable(), hook);
assert.strictEqual(hook.enable(), hook);

setImmediate(common.mustCall());

assert.strictEqual(hook.disable(), hook);
assert.strictEqual(hook.disable(), hook);
assert.strictEqual(hook.disable(), hook);
