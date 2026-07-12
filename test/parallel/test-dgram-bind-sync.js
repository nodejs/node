'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

// bindSync() binds synchronously and returns the resolved address, including
// the OS-assigned ephemeral port when port is 0.
{
  const sock = dgram.createSocket('udp4');
  const addr = sock.bindSync({ address: '127.0.0.1', port: 0 });

  assert.strictEqual(addr.address, '127.0.0.1');
  assert.strictEqual(addr.family, 'IPv4');
  assert.strictEqual(typeof addr.port, 'number');
  assert.ok(addr.port > 0);

  // address() is valid synchronously and matches the returned address.
  assert.deepStrictEqual(sock.address(), addr);

  // The 'listening' event still fires on the next tick.
  sock.on('listening', common.mustCall(() => sock.close()));
}

// Closing synchronously after bindSync() suppresses the deferred 'listening'.
{
  const sock = dgram.createSocket('udp4');
  sock.bindSync({ port: 0 });
  sock.on('listening', common.mustNotCall());
  sock.close();
}

// Defaults the address to the udp4 wildcard when omitted.
{
  const sock = dgram.createSocket('udp4');
  const addr = sock.bindSync();
  assert.strictEqual(addr.address, '0.0.0.0');
  assert.ok(addr.port > 0);
  sock.close();
}

// 'message' events still flow asynchronously after a synchronous bind.
{
  const receiver = dgram.createSocket('udp4');
  const addr = receiver.bindSync({ address: '127.0.0.1', port: 0 });

  receiver.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg.toString(), 'hello');
    receiver.close();
  }));

  const sender = dgram.createSocket('udp4');
  sender.send('hello', addr.port, '127.0.0.1', common.mustCall(() => {
    sender.close();
  }));
}

// Throws synchronously on EADDRINUSE.
{
  const first = dgram.createSocket('udp4');
  const addr = first.bindSync({ address: '127.0.0.1', port: 0 });

  const second = dgram.createSocket('udp4');
  assert.throws(() => {
    second.bindSync({ address: '127.0.0.1', port: addr.port });
  }, {
    code: 'EADDRINUSE',
    syscall: 'bind',
  });

  first.close();
  second.close();
}

// Throws synchronously on a non-numeric address (no DNS resolution).
{
  const sock = dgram.createSocket('udp4');
  assert.throws(() => {
    sock.bindSync({ address: 'localhost', port: 0 });
  }, {
    code: 'ERR_INVALID_ARG_VALUE',
    name: 'TypeError',
  });
  sock.close();
}

// Rejects a non-string address.
{
  const sock = dgram.createSocket('udp4');
  assert.throws(() => sock.bindSync({ address: 12345 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  sock.close();
}

// A rejected argument leaves the socket unbound and reusable.
{
  const sock = dgram.createSocket('udp4');
  assert.throws(() => sock.bindSync({ port: -1 }), {
    code: 'ERR_SOCKET_BAD_PORT',
  });
  const addr = sock.bindSync({ port: 0 });
  assert.ok(addr.port > 0);
  sock.close();
}

// Throws when already bound.
{
  const sock = dgram.createSocket('udp4');
  sock.bindSync({ port: 0 });
  assert.throws(() => sock.bindSync({ port: 0 }), {
    code: 'ERR_SOCKET_ALREADY_BOUND',
  });
  sock.close();
}

// Rejects a non-object options argument.
{
  const sock = dgram.createSocket('udp4');
  assert.throws(() => sock.bindSync(0), { code: 'ERR_INVALID_ARG_TYPE' });
  sock.close();
}

// udp6 wildcard default.
if (common.hasIPv6) {
  const sock = dgram.createSocket('udp6');
  const addr = sock.bindSync();
  assert.strictEqual(addr.address, '::');
  assert.strictEqual(addr.family, 'IPv6');
  assert.ok(addr.port > 0);
  sock.close();
}

// udp6 loopback with an OS-assigned ephemeral port, and async 'message' flow.
if (common.hasIPv6) {
  const receiver = dgram.createSocket('udp6');
  const addr = receiver.bindSync({ address: '::1', port: 0 });

  assert.strictEqual(addr.address, '::1');
  assert.strictEqual(addr.family, 'IPv6');
  assert.ok(addr.port > 0);
  assert.deepStrictEqual(receiver.address(), addr);

  receiver.on('message', common.mustCall((msg) => {
    assert.strictEqual(msg.toString(), 'hello');
    receiver.close();
  }));

  const sender = dgram.createSocket('udp6');
  sender.send('hello', addr.port, '::1', common.mustCall(() => {
    sender.close();
  }));
}

// A zone-indexed (scoped) IPv6 literal is accepted as a numeric IP; no DNS
// resolution occurs. Interface names are platform-specific, so this binds the
// scoped loopback only where the interface name is known (Linux: 'lo').
if (common.hasIPv6 && process.platform === 'linux') {
  const sock = dgram.createSocket('udp6');
  const addr = sock.bindSync({ address: '::1%lo', port: 0 });
  assert.strictEqual(addr.address, '::1');
  assert.strictEqual(addr.family, 'IPv6');
  assert.ok(addr.port > 0);
  sock.close();
}

// The ipv6Only flag is honored by the synchronous bind.
if (common.hasIPv6) {
  const sock = dgram.createSocket({ type: 'udp6', ipv6Only: true });
  const addr = sock.bindSync({ address: '::', port: 0 });
  assert.strictEqual(addr.family, 'IPv6');
  sock.close();
}
