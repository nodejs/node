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

assert.strictEqual(typeof socket.duration, 'bigint');
assert.strictEqual(typeof socket.boundDuration, 'bigint');
assert.strictEqual(typeof socket.listenDuration, 'bigint');
assert.strictEqual(typeof socket.bytesReceived, 'bigint');
assert.strictEqual(socket.bytesReceived, 0n);
assert.strictEqual(socket.bytesSent, 0n);
assert.strictEqual(socket.packetsReceived, 0n);
assert.strictEqual(socket.packetsSent, 0n);
assert.strictEqual(socket.serverSessions, 0n);
assert.strictEqual(socket.clientSessions, 0n);

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

[1, 1n, [], {}, null].forEach((args) => {
  assert.throws(() => socket.setServerBusy(args), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

socket.listen({ alpn: 'zzz' });
assert(socket.pending);

socket.on('ready', common.mustCall(() => {
  assert(endpoint.bound);

  // QuicSocket is already listening.
  assert.throws(() => socket.listen(), {
    code: 'ERR_QUICSOCKET_LISTENING'
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
}));

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
      code: 'ERR_QUICSOCKET_DESTROYED',
      message: `Cannot call ${op} after a QuicSocket has been destroyed`
    });
  });

  [
    'setServerBusy',
  ].forEach((op) => {
    assert.throws(() => socket[op](), {
      code: 'ERR_QUICSOCKET_DESTROYED',
      message: `Cannot call ${op} after a QuicSocket has been destroyed`
    });
  });
}));
