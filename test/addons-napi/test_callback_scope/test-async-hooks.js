'use strict';

const common = require('../../common');
const assert = require('assert');
const async_hooks = require('async_hooks');

// The async_hook that we enable would register the process.emitWarning()
// call from loading the N-API addon as asynchronous activity because
// it contains a process.nextTick() call. Monkey patch it to be a no-op
// before we load the addon in order to avoid this.
process.emitWarning = () => {};

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
  })
}).enable();

runInCallbackScope({}, 1000, 1000, () => {
  assert(insideHook);
});
