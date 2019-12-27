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

// Will throw because the QuicSocket is not bound
{
  const err = { code: 'EBADF' };
  assert.throws(() => endpoint.setTTL(1), err);
  assert.throws(() => endpoint.setMulticastTTL(1), err);
  assert.throws(() => endpoint.setBroadcast(), err);
  assert.throws(() => endpoint.setMulticastLoopback(), err);
  assert.throws(() => endpoint.setMulticastInterface('0.0.0.0'), err);
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
  assert(endpoint.bound);

  // QuicSocket is already listening.
  assert.throws(() => socket.listen(), {
    code: 'ERR_QUICSOCKET_LISTENING'
  });

  assert.strictEqual(typeof endpoint.address.address, 'string');
  assert.strictEqual(typeof endpoint.address.port, 'number');
  assert.strictEqual(typeof endpoint.address.family, 'string');

  // On Windows, fd will always be undefined.
  if (common.isWindows)
    assert.strictEqual(endpoint.fd, undefined);
  else
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
    () => endpoint.setTTL(1),
    makeError('setTTL')
  );
  assert.throws(
    () => endpoint.setMulticastTTL(1),
    makeError('setMulticastTTL')
  );
  assert.throws(
    () => endpoint.setBroadcast(true),
    makeError('setBroadcast')
  );
  assert.throws(
    () => endpoint.setMulticastLoopback(),
    makeError('setMulticastLoopback')
  );
  assert.throws(
    () => endpoint.setMulticastInterface(true),
    makeError('setMulticastInterface')
  );
  assert.throws(
    () => endpoint.addMembership('foo', 'bar'),
    makeError('addMembership')
  );
  assert.throws(
    () => endpoint.dropMembership('foo', 'bar'),
    makeError('dropMembership')
  );
  assert.throws(
    () => socket.setServerBusy(true),
    makeError('setServerBusy')
  );
}));
