// Flags: --expose-internals --no-warnings
'use strict';

// Tests QUIC server busy support

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { once } = require('events');
const { key, cert, ca } = require('../common/quic');

const { createQuicSocket } = require('net');
const options = { key, cert, ca, alpn: 'zzz' };

const client = createQuicSocket({ client: options });
const server = createQuicSocket({ server: options });

client.on('close', common.mustCall());
server.on('close', common.mustCall());
server.on('listening', common.mustCall());
server.on('busy', common.mustCall((busy) => {
  assert.strictEqual(busy, true);
}));

// When the server is set as busy, all connections
// will be rejected with a SERVER_BUSY response.
server.serverBusy = true;

(async function() {
  server.on('session', common.mustNotCall());
  await server.listen();

  const req = await client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
  });

  req.on('secure', common.mustNotCall());

  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.closeCode.code, 2);
    server.close();
    client.close();
  }));

  await Promise.all([
    once(server, 'close'),
    once(client, 'close')
  ]);
})().then(common.mustCall());
