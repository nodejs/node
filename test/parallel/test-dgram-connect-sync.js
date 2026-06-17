'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const net = require('net');

// connectSync() connects synchronously, binding the socket first when needed,
// and remoteAddress() is valid immediately.
{
  const sock = dgram.createSocket('udp4');
  sock.connectSync(12345, '127.0.0.1');

  const peer = sock.remoteAddress();
  assert.strictEqual(peer.address, '127.0.0.1');
  assert.strictEqual(peer.family, 'IPv4');
  assert.strictEqual(peer.port, 12345);

  // The socket was bound synchronously as part of connecting.
  assert.ok(sock.address().port > 0);

  // The 'connect' event still fires on the next tick.
  sock.on('connect', common.mustCall(() => sock.close()));
}

// Closing synchronously after connectSync() suppresses the deferred 'connect'.
{
  const sock = dgram.createSocket('udp4');
  sock.connectSync(12345, '127.0.0.1');
  sock.on('connect', common.mustNotCall());
  sock.close();
}

// Defaults the address to the udp4 loopback when omitted.
{
  const sock = dgram.createSocket('udp4');
  sock.connectSync(12345);
  assert.strictEqual(sock.remoteAddress().address, '127.0.0.1');
  sock.close();
}

// Works on a socket already bound with bindSync().
{
  const sock = dgram.createSocket('udp4');
  sock.bindSync({ address: '127.0.0.1', port: 0 });
  const boundPort = sock.address().port;
  sock.connectSync(12345, '127.0.0.1');
  assert.strictEqual(sock.address().port, boundPort);
  assert.strictEqual(sock.remoteAddress().port, 12345);
  sock.close();
}

// Datagrams flow to the connected peer after a synchronous connect.
{
  const receiver = dgram.createSocket('udp4');
  const addr = receiver.bindSync({ address: '127.0.0.1', port: 0 });

  receiver.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg.toString(), 'hello');
    receiver.close();
  }));

  const sender = dgram.createSocket('udp4');
  sender.connectSync(addr.port, '127.0.0.1');
  sender.send('hello', common.mustCall(() => sender.close()));
}

// disconnect() works after a synchronous connect, allowing a reconnect.
{
  const sock = dgram.createSocket('udp4');
  sock.connectSync(12345, '127.0.0.1');
  sock.disconnect();
  sock.connectSync(12346, '127.0.0.1');
  assert.strictEqual(sock.remoteAddress().port, 12346);
  sock.close();
}

// Throws synchronously on a non-numeric address (no DNS resolution).
{
  const sock = dgram.createSocket('udp4');
  assert.throws(() => {
    sock.connectSync(12345, 'localhost');
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
  });
  sock.close();
}

// Rejects a non-string address.
{
  const sock = dgram.createSocket('udp4');
  assert.throws(() => sock.connectSync(12345, 12345), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  sock.close();
}

// A rejected argument leaves the socket unbound and reusable.
{
  const sock = dgram.createSocket('udp4');
  assert.throws(() => sock.connectSync(-1, '127.0.0.1'), {
    code: 'ERR_SOCKET_BAD_PORT',
  });
  sock.connectSync(12345, '127.0.0.1');
  assert.strictEqual(sock.remoteAddress().port, 12345);
  sock.close();
}

// Throws when already connected.
{
  const sock = dgram.createSocket('udp4');
  sock.connectSync(12345, '127.0.0.1');
  assert.throws(() => sock.connectSync(12346, '127.0.0.1'), {
    code: 'ERR_SOCKET_DGRAM_IS_CONNECTED',
  });
  sock.close();
}

// Throws synchronously for a blocked address, leaving the socket reusable.
{
  const blockList = new net.BlockList();
  blockList.addAddress('127.0.0.1');
  const sock = dgram.createSocket({ type: 'udp4', sendBlockList: blockList });
  assert.throws(() => sock.connectSync(12345, '127.0.0.1'), {
    code: 'ERR_IP_BLOCKED',
  });
  sock.connectSync(12345, '127.0.0.2');
  assert.strictEqual(sock.remoteAddress().address, '127.0.0.2');
  sock.close();
}

// Throws when an asynchronous bind() is still in progress.
{
  const sock = dgram.createSocket('udp4');
  sock.bind(0, '127.0.0.1');
  assert.throws(() => sock.connectSync(12345, '127.0.0.1'), {
    code: 'ERR_SOCKET_ALREADY_BOUND',
  });
  sock.on('listening', common.mustCall(() => sock.close()));
}

// udp6 loopback default.
if (common.hasIPv6) {
  const sock = dgram.createSocket('udp6');
  sock.connectSync(12345);
  const peer = sock.remoteAddress();
  assert.strictEqual(peer.address, '::1');
  assert.strictEqual(peer.family, 'IPv6');
  assert.strictEqual(peer.port, 12345);
  sock.close();
}

// udp6 datagrams flow to the connected peer after a synchronous connect.
if (common.hasIPv6) {
  const receiver = dgram.createSocket('udp6');
  const addr = receiver.bindSync({ address: '::1', port: 0 });

  receiver.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg.toString(), 'hello');
    receiver.close();
  }));

  const sender = dgram.createSocket('udp6');
  sender.connectSync(addr.port, '::1');
  sender.send('hello', common.mustCall(() => sender.close()));
}
