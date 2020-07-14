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
    session.on('stream', common.mustCall((stream) => {
      stream.destroy();
      stream.on('close', common.mustCall());
      stream.on('error', common.mustNotCall());
      assert(stream.destroyed);
    }));
  }));

  await server.listen();

  const req = await client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port
  });

  const stream = await req.openStream();
  stream.end('foo');
  // Do not explicitly end the stream here.

  stream.resume();
  stream.on('end', common.mustCall());

  stream.on('close', common.mustCall(() => {
    assert(stream.destroyed);
    client.close();
    server.close();
  }));

  req.on('close', common.mustCall());

  await Promise.all([
    once(server, 'close'),
    once(client, 'close')
  ]);
})().then(common.mustCall());
