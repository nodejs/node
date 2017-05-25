'use strict';

const common = require('../../common');
const assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
const async_hooks = require('async_hooks');

const kObjectTag = Symbol('kObjectTag');

const bindingUids = [];
let expectedTriggerId;
let before = 0;
let after = 0;
let destroy = 0;

async_hooks.createHook({
  init(id, type, triggerId, resource) {
    assert.strictEqual(typeof id, 'number');
    assert.strictEqual(typeof resource, 'object');
    assert(id > 1);
    if (type === 'foobär') {
      assert.strictEqual(resource.kObjectTag, kObjectTag);
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

assert.strictEqual(binding.getCurrentId(), 1);

for (const call of [binding.callViaFunction,
                    binding.callViaString,
                    binding.callViaUtf8Name]) {
  for (const passedTriggerId of [undefined, 12345]) {
    let uid;
    const object = {
      methöd(arg) {
        assert.strictEqual(this, object);
        assert.strictEqual(arg, 42);
        assert.strictEqual(binding.getCurrentId(), uid);
        return 'baz';
      },
      kObjectTag
    };

    if (passedTriggerId === undefined)
      expectedTriggerId = 1;
    else
      expectedTriggerId = passedTriggerId;

    const resource = binding.createAsyncResource(object, passedTriggerId);
    uid = bindingUids[bindingUids.length - 1];

    const ret = call(resource);
    assert.strictEqual(ret, 'baz');
    assert.strictEqual(binding.getResource(resource), object);
    assert.strictEqual(binding.getUid(resource), uid);

    binding.destroyAsyncResource(resource);
  }
}

setImmediate(common.mustCall(() => {
  assert.strictEqual(bindingUids.length, 6);
  assert.strictEqual(before, bindingUids.length);
  assert.strictEqual(after, bindingUids.length);
  assert.strictEqual(destroy, bindingUids.length);
}));
