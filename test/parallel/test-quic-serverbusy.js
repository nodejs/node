// Flags: --expose-internals --no-warnings
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

let client;
const server = createSocket({
  endpoint: { port: kServerPort },
  server: { key, cert, ca, alpn: kALPN }
});

server.on('busy', common.mustCall((busy) => {
  assert.strictEqual(busy, true);
}));

// When the server is set as busy, all connections
// will be rejected with a SERVER_BUSY response.
server.setServerBusy();
server.listen();

server.on('close', common.mustCall());
server.on('listening', common.mustCall());
server.on('session', common.mustNotCall());

server.on('ready', common.mustCall(() => {
  debug('Server is listening on port %d', server.endpoints[0].address.port);
  client = createSocket({
    endpoint: { port: kClientPort },
    client: { key, cert, ca, alpn: kALPN }
  });

  client.on('close', common.mustCall());

  const req = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port,
  });

  req.on('secure', common.mustNotCall());

  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
