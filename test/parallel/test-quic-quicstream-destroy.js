// Flags: --no-warnings
'use strict';

// Test that destroying a QuicStream immediately and synchronously
// after creation does not crash the process and closes the streams
// abruptly on both ends of the connection.

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const {
  debug,
  key,
  cert,
  ca
} = require('../common/quic');

const { createQuicSocket } = require('net');

const options = { key, cert, ca, alpn: 'zzz' };

const server = createQuicSocket({ server: options });

server.listen();

server.on('session', common.mustCall((session) => {
  debug('QuicServerSession Created');

  session.on('stream', common.mustCall((stream) => {
    stream.destroy();
    stream.on('close', common.mustCall());
    stream.on('error', common.mustNotCall());
    assert(stream.destroyed);
  }));
}));

server.on('ready', common.mustCall(() => {
  debug('Server is listening on port %d', server.endpoints[0].address.port);

  const client = createQuicSocket({ client: options });

  client.on('close', common.mustCall(() => {
    debug('Client closing. Duration', client.duration);
  }));

  const req = client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port
  });

  req.on('secure', common.mustCall(() => {
    debug('QuicClientSession TLS Handshake Complete');

    const stream = req.openStream();
    stream.write('foo');
    // Do not explicitly end the stream here.

    stream.on('finish', common.mustNotCall());
    stream.on('data', common.mustNotCall());
    stream.on('end', common.mustCall());

    stream.on('close', common.mustCall(() => {
      debug('Stream closed on client side');
      assert(stream.destroyed);
      client.close();
      server.close();
    }));
  }));

  req.on('close', common.mustCall());
}));

server.on('listening', common.mustCall());
