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
  ca,
  kServerPort,
  kClientPort,
  setupKeylog
} = require('../common/quic');

const { createSocket } = require('quic');

const kServerName = 'agent2';
const kALPN = 'zzz';

const server = createSocket({ endpoint: { port: kServerPort } });

server.listen({ key, cert, ca, alpn: kALPN });

server.on('session', common.mustCall((session) => {
  debug('QuicServerSession Created');
  setupKeylog(session);

  session.on('stream', common.mustCall((stream) => {
    stream.destroy();
    stream.on('close', common.mustCall());
    stream.on('error', common.mustNotCall());
    // Abort will not be called in this case because close()
    // is not used. The stream is just immediately destroyed
    // and no longer available for use.
    stream.on('abort', common.mustNotCall());
    assert(stream.destroyed);
  }));
}));

server.on('ready', common.mustCall(() => {
  debug('Server is listening on port %d', server.endpoints[0].address.port);

  const client = createSocket({
    endpoint: { port: kClientPort },
    client: { key, cert, ca, alpn: kALPN }
  });

  client.on('close', common.mustCall(() => {
    debug('Client closing. Duration', client.duration);
  }));

  const req = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port,
    servername: kServerName,
  });

  req.on('secure', common.mustCall((servername, alpn, cipher) => {
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
