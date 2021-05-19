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

const expectedResource = {};
const expectedResourceType = 'test-resource';
let insideHook = false;
let expectedId;
async_hooks.createHook({
  init: common.mustCall((id, type, triggerAsyncId, resource) => {
    if (type !== expectedResourceType) {
      return;
    }
    assert.strictEqual(resource, expectedResource);
    expectedId = id;
  }),
  before: common.mustCall((id) => {
    assert.strictEqual(id, expectedId);
    insideHook = true;
  }),
  after: common.mustCall((id) => {
    assert.strictEqual(id, expectedId);
    insideHook = false;
  }),
}).enable();

runInCallbackScope(expectedResource, expectedResourceType, () => {
  assert(insideHook);
});
