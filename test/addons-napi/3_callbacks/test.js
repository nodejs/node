'use strict';
const common = require('../../common');
const assert = require('assert');
const addon = require(`./build/${common.buildType}/binding`);

addon.RunCallback(function(msg) {
  assert.strictEqual(msg, 'hello world');
});

const global = function() { return this; }.apply();

function testRecv(desiredRecv) {
  addon.RunCallbackWithRecv(function() {
    assert.strictEqual(this,
                       (desiredRecv === undefined ||
                        desiredRecv === null) ?
                       global : desiredRecv);
  }, desiredRecv);
}

testRecv(undefined);
testRecv(null);
testRecv(5);
testRecv(true);
testRecv('Hello');
testRecv([]);
testRecv({});
