// Flags: --expose-internals --no-warnings
'use strict';

// Testing stateless reset

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { internalBinding } = require('internal/test/binding');
const assert = require('assert');

const { key, cert, ca } = require('../common/quic');

const {
  kHandle,
} = require('internal/stream_base_commons');
const { silentCloseSession } = internalBinding('quic');

const { createQuicSocket } = require('net');

const kStatelessResetToken =
  Buffer.from('000102030405060708090A0B0C0D0E0F', 'hex');

const options = { key, cert, ca, alpn: 'zzz' };

const client = createQuicSocket({ client: options });
const server = createQuicSocket({
  statelessResetSecret: kStatelessResetToken,
  server: options
});

server.on('close', common.mustCall(() => {
  // Verify stats recording
  assert(server.statelessResetCount >= 1);
}));

(async function() {
  server.on('session', common.mustCall((session) => {
    session.on('stream', common.mustCall((stream) => {
      // silentCloseSession is an internal-only testing tool
      // that allows us to prematurely destroy a QuicSession
      // without the proper communication flow with the connected
      // peer. We call this to simulate a local crash that loses
      // state, which should trigger the server to send a
      // stateless reset token to the client.
      silentCloseSession(session[kHandle]);
    }));

    session.on('close', common.mustCall());
  }));

  await server.listen();

  const req = await client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port,
  });

  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.statelessReset, true);
    server.close();
    client.close();
  }));

  const stream = await req.openStream();
  stream.end('hello');
  stream.resume();
  stream.on('close', common.mustCall());

})().then(common.mustCall());
