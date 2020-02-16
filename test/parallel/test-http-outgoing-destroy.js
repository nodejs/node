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
  msg.write('asd', common.expectsError({
    name: 'Error',
    code: 'ERR_STREAM_DESTROYED'
  }));
  msg.on('error', common.expectsError({
    name: 'Error',
    code: 'ERR_STREAM_DESTROYED'
  }));
}

{
  const msg = new OutgoingMessage();
  assert.strictEqual(msg.destroyed, false);
  msg.destroy();
  assert.strictEqual(msg.destroyed, true);
  msg.write('end', common.expectsError({
    name: 'Error',
    code: 'ERR_STREAM_DESTROYED'
  }));
  msg.on('error', common.expectsError({
    name: 'Error',
    code: 'ERR_STREAM_DESTROYED'
  }));
}
