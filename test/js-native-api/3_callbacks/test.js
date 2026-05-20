'use strict';
const common = require('../../common');
const assert = require('assert');
const addon = require(`./build/${common.buildType}/3_callbacks`);

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
