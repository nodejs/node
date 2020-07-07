// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { internalBinding } = require('internal/test/binding');
const {
  constants: {
    NGTCP2_CONNECTION_REFUSED
  }
} = internalBinding('quic');

const assert = require('assert');
const {
  key,
  cert,
  ca,
} = require('../common/quic');

const { createQuicSocket } = require('net');

const options = { key, cert, ca, alpn: 'zzz' };

const server = createQuicSocket({ server: options });

server.on('session', common.mustCall(async (session) => {
  session.on('close', common.mustCall());
  session.on('error', common.mustCall((err) => {
    assert.strictEqual(err.message, 'boom');
  }));
  // Throwing inside the session event handler should cause
  // the session to be destroyed immediately. This should
  // cause the client side to be closed also.
  throw new Error('boom');
}));

server.on('sessionError', common.mustCall((err, session) => {
  assert.strictEqual(err.message, 'boom');
  assert(session.destroyed);
}));

server.listen();

server.once('listening', common.mustCall(() => {
  const client = createQuicSocket({ client: options });
  const req = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port
  });
  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.closeCode.code, NGTCP2_CONNECTION_REFUSED);
    assert.strictEqual(req.closeCode.silent, true);
    server.close();
    client.close();
  }));
}));
