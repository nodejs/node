// Flags: --no-warnings
'use strict';

// Test QuicSocket constructor option errors.

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');

const { createQuicSocket } = require('net');

const socket = createQuicSocket();
assert(socket);

// Before listen is called, serverSecureContext is always undefined.
assert.strictEqual(socket.serverSecureContext, undefined);

assert.deepStrictEqual(socket.endpoints.length, 1);

// Socket is not bound, so address should be empty
assert.deepStrictEqual(socket.endpoints[0].address, {});

// Socket is not bound
assert(!socket.bound);

// Socket is not pending
assert(!socket.pending);

// Socket is not destroyed
assert(!socket.destroyed);

assert.strictEqual(typeof socket.duration, 'number');
assert.strictEqual(typeof socket.boundDuration, 'number');
assert.strictEqual(typeof socket.listenDuration, 'number');
assert.strictEqual(typeof socket.bytesReceived, 'number');
assert.strictEqual(socket.bytesReceived, 0);
assert.strictEqual(socket.bytesSent, 0);
assert.strictEqual(socket.packetsReceived, 0);
assert.strictEqual(socket.packetsSent, 0);
assert.strictEqual(socket.serverSessions, 0);
assert.strictEqual(socket.clientSessions, 0);

const endpoint = socket.endpoints[0];
assert(endpoint);

// Will throw because the QuicSocket is not bound
{
  const err = { code: 'EBADF' };
  assert.throws(() => endpoint.setTTL(1), err);
  assert.throws(() => endpoint.setMulticastTTL(1), err);
  assert.throws(() => endpoint.setBroadcast(), err);
  assert.throws(() => endpoint.setMulticastLoopback(), err);
  assert.throws(() => endpoint.setMulticastInterface('0.0.0.0'), err);
  // TODO(@jasnell): Verify behavior of add/drop membership then test
  // assert.throws(() => endpoint.addMembership(
  //     '127.0.0.1', '127.0.0.1'), err);
  // assert.throws(() => endpoint.dropMembership(
  //     '127.0.0.1', '127.0.0.1'), err);
}

['test', null, {}, [], 1n, false].forEach((rx) => {
  assert.throws(() => socket.setDiagnosticPacketLoss({ rx }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

['test', null, {}, [], 1n, false].forEach((tx) => {
  assert.throws(() => socket.setDiagnosticPacketLoss({ tx }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

[
  { rx: -1 },
  { rx: 1.1 },
  { tx: -1 },
  { tx: 1.1 }
].forEach((options) => {
  assert.throws(() => socket.setDiagnosticPacketLoss(options), {
    code: 'ERR_OUT_OF_RANGE'
  });
});

[1, 1n, [], {}, null].forEach((arg) => {
  assert.throws(() => socket.serverBusy = arg, {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

(async function() {
  const p = socket.listen({ alpn: 'zzz' });
  assert(socket.pending);

  await p;

  assert(endpoint.bound);

  // QuicSocket is already listening.
  await assert.rejects(socket.listen(), {
    code: 'ERR_INVALID_STATE'
  });

  assert.strictEqual(typeof endpoint.address.address, 'string');
  assert.strictEqual(typeof endpoint.address.port, 'number');
  assert.strictEqual(typeof endpoint.address.family, 'string');

  if (!common.isWindows)
    assert.strictEqual(typeof endpoint.fd, 'number');

  endpoint.setTTL(1);
  endpoint.setMulticastTTL(1);
  endpoint.setBroadcast();
  endpoint.setBroadcast(true);
  endpoint.setBroadcast(false);

  endpoint.setMulticastLoopback();
  endpoint.setMulticastLoopback(true);
  endpoint.setMulticastLoopback(false);

  endpoint.setMulticastInterface('0.0.0.0');

  socket.setDiagnosticPacketLoss({ rx: 0.5, tx: 0.5 });

  socket.destroy();
  assert(socket.destroyed);
})().then(common.mustCall());

socket.on('close', common.mustCall(() => {
  [
    'ref',
    'unref',
    'setTTL',
    'setMulticastTTL',
    'setBroadcast',
    'setMulticastLoopback',
    'setMulticastInterface',
    'addMembership',
    'dropMembership'
  ].forEach((op) => {
    assert.throws(() => endpoint[op](), {
      code: 'ERR_INVALID_STATE'
    });
  });

  assert.throws(() => { socket.serverBusy = true; }, {
    code: 'ERR_INVALID_STATE'
  });
}));
