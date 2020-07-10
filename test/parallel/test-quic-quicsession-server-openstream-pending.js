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
    // The server can create a stream immediately without waiting
    // for the secure event... however, the data will not actually
    // be transmitted until the handshake is completed.
    const stream = session.openStream({ halfOpen: true });
    stream.on('close', common.mustCall());
    stream.on('error', console.log);
    stream.end('hello');

    session.on('stream', common.mustNotCall());
  }));

  await server.listen();

  const req = await client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
  });

  const [ stream ] = await once(req, 'stream');

  let data = '';
  stream.setEncoding('utf8');
  stream.on('data', (chunk) => data += chunk);
  stream.on('end', common.mustCall());

  await once(stream, 'close');

  assert.strictEqual(data, 'hello');

  server.close();
  client.close();

  await Promise.all([
    once(server, 'close'),
    once(client, 'close')
  ]);

})().then(common.mustCall());
