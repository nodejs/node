// Flags: --expose-internals --no-warnings
'use strict';

// Tests QUIC server busy support

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const {
  key,
  cert,
  ca,
  debug,
  kServerPort,
  kClientPort
} = require('../common/quic');

const { createSocket } = require('quic');

let client;
const server = createSocket({
  endpoint: { port: kServerPort },
  server: { key, cert, ca, alpn: 'zzz' }
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
    client: { key, cert, ca, alpn: 'zzz' }
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
