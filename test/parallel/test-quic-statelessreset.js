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

let client;

const server = createQuicSocket({ statelessResetSecret: kStatelessResetToken });

server.listen({ key, cert, ca, alpn: 'zzz' });

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

server.on('close', common.mustCall(() => {
  // Verify stats recording
  console.log(server.statelessResetCount);
  assert(server.statelessResetCount >= 1n);
}));

server.on('ready', common.mustCall(() => {
  const endpoint = server.endpoints[0];

  client = createQuicSocket({ client: { key, cert, ca, alpn: 'zzz' } });

  client.on('close', common.mustCall());

  const req = client.connect({
    address: 'localhost',
    port: endpoint.address.port,
    servername: 'localhost',
  });

  req.on('secure', common.mustCall(() => {
    const stream = req.openStream();
    stream.end('hello');
    stream.resume();
    stream.on('close', common.mustCall());
  }));

  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.statelessReset, true);
    server.close();
    client.close();
  }));

}));
