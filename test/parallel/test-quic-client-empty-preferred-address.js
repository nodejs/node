// Flags: --no-warnings
'use strict';

// This test ensures that when we don't define `preferredAddress`
// on the server while the `preferredAddressPolicy` on the client
// is `accept`, it works as expected.

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { key, cert, ca } = require('../common/quic');
const { createQuicSocket } = require('net');
const { once } = require('events');

const options = { key, cert, ca, alpn: 'zzz' };

(async () => {
  const server = createQuicSocket({ server: options });
  const client = createQuicSocket({ client: options });

  server.on('session', common.mustCall((serverSession) => {
    serverSession.on('stream', common.mustCall(async (stream) => {
      stream.on('data', common.mustCall((data) => {
        assert.strictEqual(data.toString('utf8'), 'hello');
      }));

      await once(stream, 'end');

      stream.close();
      client.close();
      server.close();
    }));
  }));

  await server.listen();

  const clientSession = await client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
    preferredAddressPolicy: 'accept',
  });

  const stream = await clientSession.openStream();
  stream.end('hello');

  await Promise.all([
    once(stream, 'close'),
    once(client, 'close'),
    once(server, 'close')]);
})().then(common.mustCall());
