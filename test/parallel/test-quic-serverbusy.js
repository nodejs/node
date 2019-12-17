// Flags: --expose-internals
'use strict';

// Tests QUIC server busy support

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');

const { debuglog } = require('util');
const debug = debuglog('test');

const { createSocket } = require('quic');

const kServerPort = process.env.NODE_DEBUG_KEYLOG ? 5678 : 0;
const kClientPort = process.env.NODE_DEBUG_KEYLOG ? 5679 : 0;
const kALPN = 'zzz';  // ALPN can be overriden to whatever we want

// TODO(@jasnell): Implementation of this test is not yet complete.
// Once the feature is fully implemented, this test will need to be
// revisited.

let client;
const server = createSocket({
  endpoint: { port: kServerPort },
  server: { key, cert, ca, alpn: kALPN }
});

server.on('busy', common.mustCall((busy) => {
  assert.strictEqual(busy, true);
}));

server.setServerBusy();
server.listen();

server.on('session', common.mustCall((session) => {
  session.on('stream', common.mustNotCall());
  session.on('close', common.mustCall());
}));

server.on('ready', common.mustCall(() => {
  debug('Server is listening on port %d', server.address.port);
  client = createSocket({
    endpoint: { port: kClientPort },
    client: { key, cert, ca, alpn: kALPN }
  });

  client.on('close', common.mustCall());

  const req = client.connect({
    address: 'localhost',
    port: server.address.port,
  });

  // The client session is going to be destroyed before
  // the handshake can complete so the secure event will
  // never emit.
  req.on('secure', common.mustNotCall());

  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));

server.on('listening', common.mustCall());

server.on('close', common.mustCall(() => {
  assert.throws(
    () => server.listen(),
    {
      code: 'ERR_QUICSOCKET_DESTROYED',
      message: 'Cannot call listen after a QuicSocket has been destroyed'
    }
  );
}));
