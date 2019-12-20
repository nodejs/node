'use strict';

// Test that destroying a QuicStream immediately and synchronously
// after creation does not crash the process and closes the streams
// abruptly on both ends of the connection.

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const fs = require('fs');
const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');
const { debuglog } = require('util');
const debug = debuglog('test');

const { createSocket } = require('quic');

const kServerPort = process.env.NODE_DEBUG_KEYLOG ? 5678 : 0;
const kClientPort = process.env.NODE_DEBUG_KEYLOG ? 5679 : 0;

const kServerName = 'agent2';  // Intentionally the wrong servername
const kALPN = 'zzz';  // ALPN can be overriden to whatever we want

const server = createSocket({ endpoint: { port: kServerPort } });

server.listen({ key, cert, ca, alpn: kALPN });

server.on('session', common.mustCall((session) => {
  debug('QuicServerSession Created');

  if (process.env.NODE_DEBUG_KEYLOG) {
    const kl = fs.createWriteStream(process.env.NODE_DEBUG_KEYLOG);
    session.on('keylog', kl.write.bind(kl));
  }

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
