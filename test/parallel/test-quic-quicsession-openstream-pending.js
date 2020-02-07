// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// Test that opening a stream works even if the session isn’t ready yet.

const assert = require('assert');
const quic = require('quic');
const { key, cert, ca } = require('../common/quic');
const { once } = require('events');
const options = { key, cert, ca, alpn: 'meow' };

(async () => {
  const server = quic.createSocket({ server: options });
  const client = quic.createSocket({ client: options });

  server.listen();

  server.on('session', common.mustCall((session) => {
    session.on('stream', common.mustCall(async (stream) => {
      let data = '';
      stream.setEncoding('utf8');
      stream.on('data', (chunk) => data += chunk);
      await once(stream, 'end');
      assert.strictEqual(data, 'Hello!');
      server.close();
      client.close();
    }));
  }));

  await once(server, 'ready');

  const req = client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port
  });

  const stream = req.openStream({ halfOpen: true });
  stream.end('Hello!');

  assert.strictEqual(stream.pending, true);

  await once(stream, 'ready');

  assert.strictEqual(stream.pending, false);

  await once(stream, 'close');

  await Promise.allSettled([
    once(server, 'close'),
    once(client, 'close')
  ]);
})().then(common.mustCall());
