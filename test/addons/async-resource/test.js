'use strict';

const common = require('../../common');
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
const async_hooks = require('async_hooks');

const bindingUids = [];
let expectedTriggerId;
let before = 0;
let after = 0;
let destroy = 0;

async_hooks.createHook({
  init(id, type, triggerId, resource) {
    if (type === 'foobär') {
      assert.strictEqual(triggerId, expectedTriggerId);
      bindingUids.push(id);
    }
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
  for (const passedTriggerId of [undefined, 12345]) {
    const object = {
      methöd(arg) {
        assert.strictEqual(this, object);
        assert.strictEqual(arg, 42);
        return 'baz';
      }
    };

    if (passedTriggerId === undefined)
      expectedTriggerId = 1;
    else
      expectedTriggerId = passedTriggerId;

    const resource = binding.createAsyncResource(object, passedTriggerId);
    const ret = call(resource);
    assert.strictEqual(ret, 'baz');
    assert.strictEqual(binding.getResource(resource), object);

    const mostRecentUid = bindingUids[bindingUids.length - 1];
    assert.strictEqual(binding.getUid(resource), mostRecentUid);

    binding.destroyAsyncResource(resource);
  }
}

setImmediate(common.mustCall(() => {
  assert.strictEqual(bindingUids.length, 6);
  assert.strictEqual(before, bindingUids.length);
  assert.strictEqual(after, bindingUids.length);
  assert.strictEqual(destroy, bindingUids.length);
}));
