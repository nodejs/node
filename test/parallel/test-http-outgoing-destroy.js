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
  msg.write('asd', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_DESTROYED');
  }));
  msg.on('error', common.mustNotCall());
}

{
  const msg = new OutgoingMessage();
  msg.destroy();

  assert.strictEqual(msg._writeRaw('asd', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_DESTROYED');
  })), false);
  msg.on('error', common.mustNotCall());
}

{
  const msg = new OutgoingMessage();
  const destroyError = new Error('destroyed');
  msg.destroy(destroyError);

  assert.strictEqual(msg._writeRaw('asd', common.mustCall((err) => {
    assert.strictEqual(err, destroyError);
  })), false);
  msg.on('error', common.mustNotCall());
}

{
  const msg = new OutgoingMessage();
  msg.socket = { destroyed: true };

  assert.strictEqual(msg._writeRaw('asd', common.mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_STREAM_DESTROYED');
  })), false);
  msg.on('error', common.mustNotCall());
}
