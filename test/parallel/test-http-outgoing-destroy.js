'use strict';
const common = require('../common');
const assert = require('assert');

const http = require('http');
const OutgoingMessage = http.OutgoingMessage;

{
  const msg = new OutgoingMessage();
  assert.strictEqual(msg.destroyed, false);
  msg.destroy();
  assert.strictEqual(msg.destroyed, true);
  let callbackCalled = false;
  msg.write('asd', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_DESTROYED');
    callbackCalled = true;
  }));
  msg.on('error', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_DESTROYED');
    assert.strictEqual(callbackCalled, true);
  }));
}
