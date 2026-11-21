'use strict';

const common = require('../../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');
const binding = require(`./build/${common.buildType}/binding`);

// AsyncResource::MakeCallback() must run the callback in the async context
// frame that was active when the resource was created.

const als = new AsyncLocalStorage();
const object = {
  'methöd': common.mustCall(() => {
    assert.strictEqual(als.getStore(), 'store');
  }, 3),
};
const resource = als.run('store', () => binding.createAsyncResource(object));

binding.callViaFunction(resource);
binding.callViaString(resource);
binding.callViaUtf8Name(resource);
binding.destroyAsyncResource(resource);

// A getter of the method runs in that frame too, and what it enters stays
// there.
{
  let getterStore;
  const object = Object.defineProperty({}, 'methöd', {
    get: common.mustCall(() => {
      getterStore = als.getStore();
      als.enterWith('getter');
      return common.mustCall();
    }),
  });
  const resource =
    als.run('resource', () => binding.createAsyncResource(object));
  let callerStore;
  als.run('caller', () => {
    binding.callViaString(resource);
    callerStore = als.getStore();
  });
  binding.destroyAsyncResource(resource);
  assert.deepStrictEqual([getterStore, callerStore], ['resource', 'caller']);
}
