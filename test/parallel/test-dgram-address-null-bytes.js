'use strict';
const common = require('../common');

// An address with a null byte must be rejected synchronously by send() and
// connect(), also on a socket that is not bound yet. Previously the operation
// was queued until the implicit bind completed, and the lookup then threw from
// inside the 'listening' event, where the caller could not catch it.

const assert = require('assert');
const dgram = require('dgram');

const PORT = 12345;
const address = '127.0.0.1\u0000.example.com';
const expectedError = { code: 'ERR_INVALID_ARG_VALUE', name: 'TypeError' };

{
  const socket = dgram.createSocket('udp4');
  socket.on('error', common.mustNotCall());

  assert.throws(() => {
    socket.send('hello', PORT, address, common.mustNotCall());
  }, expectedError);

  // The socket is still usable.
  const receiver = dgram.createSocket('udp4');
  receiver.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg.toString(), 'hello');
    receiver.close();
    socket.close();
  }));
  receiver.bind(0, '127.0.0.1', common.mustCall(() => {
    socket.send('hello', receiver.address().port, '127.0.0.1',
                common.mustSucceed());
  }));
}

{
  const socket = dgram.createSocket('udp4');
  socket.on('error', common.mustNotCall());

  assert.throws(() => {
    socket.connect(PORT, address, common.mustNotCall());
  }, expectedError);

  // The failed connect() does not leave the socket in the connecting state.
  socket.connect(PORT, '127.0.0.1', common.mustCall(() => {
    socket.close();
  }));
}
