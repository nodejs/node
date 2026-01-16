'use strict';
// Addons: 3_callbacks, 3_callbacks_vtable

const common = require('../../common');
const { addonPath } = require('../../common/addon-test');
const assert = require('assert');
const addon = require(addonPath);

addon.RunCallback(common.mustCall((msg) => {
  assert.strictEqual(msg, 'hello world');
}));

function testRecv(desiredRecv) {
  addon.RunCallbackWithRecv(common.mustCall(function() {
    assert.strictEqual(this, desiredRecv);
  }), desiredRecv);
}

testRecv(undefined);
testRecv(null);
testRecv(5);
testRecv(true);
testRecv('Hello');
testRecv([]);
testRecv({});
