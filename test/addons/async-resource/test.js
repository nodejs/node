'use strict';

const common = require('../../common');
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
const async_hooks = require('async_hooks');

const bindingUids = [];
let before = 0;
let after = 0;
let destroy = 0;

async_hooks.createHook({
  init(id, type, triggerId, resource) {
    if (type === 'foobär') bindingUids.push(id);
  },

  before(id) {
    if (bindingUids.includes(id)) before++;
  },

  after(id) {
    if (bindingUids.includes(id)) after++;
  },

  destroy(id) {
    if (bindingUids.includes(id)) destroy++;
  }
}).enable();

for (const call of [binding.callViaFunction,
                    binding.callViaString,
                    binding.callViaUtf8Name]) {
  const object = {
    methöd(arg) {
      assert.strictEqual(this, object);
      assert.strictEqual(arg, 42);
      return 'baz';
    }
  };

  const resource = binding.createAsyncResource(object);
  const ret = call(resource);
  assert.strictEqual(ret, 'baz');
  binding.destroyAsyncResource(resource);
}

setImmediate(common.mustCall(() => {
  assert.strictEqual(bindingUids.length, 3);
  assert.strictEqual(before, 3);
  assert.strictEqual(after, 3);
  assert.strictEqual(destroy, 3);
}));
