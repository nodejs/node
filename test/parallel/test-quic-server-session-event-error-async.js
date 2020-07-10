// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { once } = require('events');
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

const client = createQuicSocket({ client: options });
const server = createQuicSocket({ server: options });

(async function() {
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

  await server.listen();

  const req = await client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port
  });

  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.closeCode.code, NGTCP2_CONNECTION_REFUSED);
    assert.strictEqual(req.closeCode.silent, true);
    server.close();
    client.close();
  }));

  await Promise.all([
    once(server, 'close'),
    once(client, 'close')
  ]);
})().then(common.mustCall());
