// Flags: --no-warnings
'use strict';

// Test that destroying a QuicStream immediately and synchronously
// after creation does not crash the process and closes the streams
// abruptly on both ends of the connection.

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

(async function() {
  server.on('session', common.mustCall((session) => {
    session.on('stream', common.mustNotCall());
    session.on('close', common.mustCall(async () => {
      await Promise.all([
        client.close(),
        server.close()
      ]);
      assert.rejects(server.close(), {
        code: 'ERR_INVALID_STATE',
        name: 'Error'
      });
    }));
    session.destroy();
  }));

  await server.listen();

  const req = await client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port
  });
  req.on('secure', common.mustNotCall());
  req.on('close', common.mustCall());

  await Promise.all([
    once(server, 'close'),
    once(client, 'close')
  ]);

})().then(common.mustCall());
