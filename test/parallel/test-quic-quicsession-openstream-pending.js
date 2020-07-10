// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// Test that opening a stream works even if the session isn’t ready yet.

const assert = require('assert');
const { createQuicSocket } = require('net');
const { key, cert, ca } = require('../common/quic');
const { once } = require('events');
const options = { key, cert, ca, alpn: 'meow' };

const server = createQuicSocket({ server: options });
const client = createQuicSocket({ client: options });

(async () => {
  server.on('session', common.mustCall((session) => {
    session.on('stream', common.mustCall(async (stream) => {
      let data = '';
      stream.setEncoding('utf8');
      stream.on('data', (chunk) => data += chunk);
      await once(stream, 'end');
      assert.strictEqual(data, 'Hello!');
    }));
  }));

  await server.listen();

  const req = await client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port
  });

  // In this case, the QuicStream is usable but corked
  // until the underlying internal QuicStream handle
  // has been created, which will not happen until
  // after the TLS handshake has been completed.
  const stream = req.openStream({ halfOpen: true });
  stream.end('Hello!');
  stream.on('error', common.mustNotCall());
  stream.resume();
  assert(!req.allowEarlyData);
  assert(!req.handshakeComplete);
  assert(stream.pending);

  await once(stream, 'ready');

  assert(req.handshakeComplete);
  assert(!stream.pending);

  await once(stream, 'close');

  server.close();
  client.close();

  await Promise.all([
    once(server, 'close'),
    once(client, 'close')
  ]);
})().then(common.mustCall());
