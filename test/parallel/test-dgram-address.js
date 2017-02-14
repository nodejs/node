'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

{
  // IPv4 Test
  const socket = dgram.createSocket('udp4');

  socket.on('listening', common.mustCall(() => {
    const address = socket.address();

    assert.strictEqual(address.address, common.localhostIPv4);
    assert.strictEqual(typeof address.port, 'number');
    assert.ok(isFinite(address.port));
    assert.ok(address.port > 0);
    assert.strictEqual(address.family, 'IPv4');
    socket.close();
  }));

  socket.on('error', (err) => {
    socket.close();
    common.fail(`Unexpected error on udp4 socket. ${err.toString()}`);
  });

  socket.bind(0, common.localhostIPv4);
}

{
  // IPv6 Test
  const socket = dgram.createSocket('udp6');
  const localhost = '::1';

  socket.on('listening', common.mustCall(() => {
    const address = socket.address();

    assert.strictEqual(address.address, localhost);
    assert.strictEqual(typeof address.port, 'number');
    assert.ok(isFinite(address.port));
    assert.ok(address.port > 0);
    assert.strictEqual(address.family, 'IPv6');
    socket.close();
  }));

  socket.on('error', (err) => {
    socket.close();
    common.fail(`Unexpected error on udp6 socket. ${err.toString()}`);
  });

  socket.bind(0, localhost);
}

{
  // Verify that address() throws if the socket is not bound.
  const socket = dgram.createSocket('udp4');

  assert.throws(() => {
    socket.address();
  }, /^Error: getsockname EINVAL$/);
}
