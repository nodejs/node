// Flags: --no-warnings
'use strict';

// Test QuicSocket constructor option errors.

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');

const { createSocket } = require('quic');

const socket = createSocket();
assert(socket);

// Before listen is called, serverSecureContext is always undefined.
assert.strictEqual(socket.serverSecureContext, undefined);

// Socket is not bound, so address should be empty
assert.deepStrictEqual(socket.address, {});

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

// Will throw because the QuicSocket is not bound
{
  const err = { code: 'EBADF' };
  assert.throws(() => socket.setTTL(1), err);
  assert.throws(() => socket.setMulticastTTL(1), err);
  assert.throws(() => socket.setBroadcast(), err);
  assert.throws(() => socket.setMulticastLoopback(), err);
  assert.throws(() => socket.setMulticastInterface('0.0.0.0'), err);
  // assert.throws(() => socket.addMembership('127.0.0.1', '127.0.0.1'), err);
  // assert.throws(() => socket.dropMembership('127.0.0.1', '127.0.0.1'), err);
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

assert.throws(() => socket.setDiagnosticPacketLoss({ rx: -1 }), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => socket.setDiagnosticPacketLoss({ rx: 1.1 }), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => socket.setDiagnosticPacketLoss({ tx: -1 }), {
  code: 'ERR_OUT_OF_RANGE'
});

assert.throws(() => socket.setDiagnosticPacketLoss({ tx: 1.1 }), {
  code: 'ERR_OUT_OF_RANGE'
});

[1, 1n, false, [], {}, null].forEach((alpn) => {
  assert.throws(() => socket.listen({ alpn }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

[1, 1n, false, [], {}, null].forEach((ciphers) => {
  assert.throws(() => socket.listen({ ciphers }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

[1, 1n, false, [], {}, null].forEach((groups) => {
  assert.throws(() => socket.listen({ groups }), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

[1, 1n, [], {}, null].forEach((args) => {
  const message = 'The "on" argument must be of type boolean.' +
                  ' Received type ' + typeof args;

  assert.throws(() => socket.setServerBusy(args), {
    code: 'ERR_INVALID_ARG_TYPE',
    message
  });
});

socket.listen({ alpn: 'zzz' });
assert(socket.pending);

socket.on('ready', common.mustCall(() => {
  assert(socket.bound);

  // QuicSocket is already listening.
  assert.throws(() => socket.listen(), {
    code: 'ERR_QUICSOCKET_LISTENING'
  });

  assert.strictEqual(typeof socket.address.address, 'string');
  assert.strictEqual(typeof socket.address.port, 'number');
  assert.strictEqual(typeof socket.address.family, 'string');

  // On Windows, fd will always be undefined.
  if (common.isWindows)
    assert.strictEqual(socket.fd, undefined);
  else
    assert.strictEqual(typeof socket.fd, 'number');

  socket.setTTL(1);
  socket.setMulticastTTL(1);
  socket.setBroadcast();
  socket.setBroadcast(true);
  socket.setBroadcast(false);

  socket.setMulticastLoopback();
  socket.setMulticastLoopback(true);
  socket.setMulticastLoopback(false);

  socket.setMulticastInterface('0.0.0.0');

  socket.setDiagnosticPacketLoss({ rx: 0.5, tx: 0.5 });

  socket.destroy();
  assert(socket.destroyed);
}));

socket.on('close', common.mustCall(() => {
  const makeError = (method) => ({
    code: 'ERR_QUICSOCKET_DESTROYED',
    message: `Cannot call ${method} after a QuicSocket has been destroyed`
  });

  assert.throws(
    () => socket.ref(),
    makeError('ref')
  );
  assert.throws(
    () => socket.unref(),
    makeError('unref')
  );
  assert.throws(
    () => socket.setTTL(1),
    makeError('setTTL')
  );
  assert.throws(
    () => socket.setMulticastTTL(1),
    makeError('setMulticastTTL')
  );
  assert.throws(
    () => socket.setBroadcast(true),
    makeError('setBroadcast')
  );
  assert.throws(
    () => socket.setMulticastLoopback(),
    makeError('setMulticastLoopback')
  );
  assert.throws(
    () => socket.setMulticastInterface(true),
    makeError('setMulticastInterface')
  );
  assert.throws(
    () => socket.addMembership('foo', 'bar'),
    makeError('addMembership')
  );
  assert.throws(
    () => socket.dropMembership('foo', 'bar'),
    makeError('dropMembership')
  );
  assert.throws(
    () => socket.setServerBusy(true),
    makeError('setServerBusy')
  );
}));
