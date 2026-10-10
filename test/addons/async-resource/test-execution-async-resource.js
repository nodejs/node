'use strict';

const common = require('../../common');
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
const { executionAsyncId, executionAsyncResource } = require('async_hooks');

// No hook is enabled and executionAsyncResource() was never called, so the
// callback scope of AsyncResource::MakeCallback() skips the async id stack.
// executionAsyncResource() must still find the resource, and executionAsyncId()
// must still be the id of the callback, also around a nested callback.
let calls = 0;
const object = {
  methöd: common.mustCall(function() {
    assert.strictEqual(executionAsyncId(), uid);
    assert.strictEqual(executionAsyncResource(), object);
    if (calls++ === 0) {
      assert.strictEqual(binding.callViaFunction(resource), 'baz');
      assert.strictEqual(executionAsyncId(), uid);
      assert.strictEqual(executionAsyncResource(), object);
    }
    return 'baz';
  }, 2),
};
const resource = binding.createAsyncResource(object);
const uid = binding.getAsyncId(resource);
const outerId = executionAsyncId();

assert.strictEqual(binding.callViaFunction(resource), 'baz');
assert.strictEqual(executionAsyncId(), outerId);
binding.destroyAsyncResource(resource);
