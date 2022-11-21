'use strict';

const common = require('../../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { runInCallbackScope } = require(`./build/${common.buildType}/binding`);

let insideHook = false;
async_hooks.createHook({
  before: common.mustCall((id) => {
    assert.strictEqual(id, 1000);
    insideHook = true;
  }),
  after: common.mustCall((id) => {
    assert.strictEqual(id, 1000);
    insideHook = false;
  }),
}).enable();

runInCallbackScope({}, 1000, 1000, () => {
  assert(insideHook);
});
