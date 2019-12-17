// Flags: --expose-internals
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const Countdown = require('../common/countdown');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');
const { debuglog } = require('util');
const debug = debuglog('test');
const fs = require('fs');

const { createSocket } = require('quic');

const kServerPort = process.env.NODE_DEBUG_KEYLOG ? 5678 : 0;
const kClientPort = process.env.NODE_DEBUG_KEYLOG ? 5679 : 0;

let client;
const server = createSocket({ endpoint: { port: kServerPort } });

const kServerName = 'agent1';
const kALPN = 'zzz';

const countdown = new Countdown(2, () => {
  debug('Countdown expired. Destroying sockets');
  server.close();
  client.close();
});

server.listen({ key, cert, ca, alpn: kALPN });

server.on('session', common.mustCall((session) => {
  debug('QuicServerSession Created');

  if (process.env.NODE_DEBUG_KEYLOG) {
    const kl = fs.createWriteStream(process.env.NODE_DEBUG_KEYLOG);
    session.on('keylog', kl.write.bind(kl));
  }

  session.on('secure', common.mustCall((servername, alpn, cipher) => {
    const uni = session.openStream({ halfOpen: true });
    uni.write('hi', common.mustCall());
    uni.close(3);

    uni.on('abort', common.mustCall((code, finalSize) => {
      debug('Undirectional, Server-initiated stream %d aborted', uni.id);
      assert.strictEqual(code, 3);
      assert.strictEqual(finalSize, 2);
    }));

    uni.on('data', common.mustNotCall());

    uni.on('end', common.mustCall(() => {
      debug('Undirectional, Server-initiated stream %d ended on server',
            uni.id);
    }));
    uni.on('close', common.mustCall(() => {
      debug('Unidirectional, Server-initiated stream %d closed on server',
            uni.id);
    }));
    uni.on('error', common.mustNotCall());

    debug('Unidirectional, Server-initiated stream %d opened', uni.id);
  }));

  session.on('stream', common.mustNotCall());
  session.on('close', common.mustCall());
}));

server.on('ready', common.mustCall(() => {
  debug('Server is listening on port %d', server.address.port);
  client = createSocket({
    endpoint: { port: kClientPort },
    client: { key, cert, ca, alpn: kALPN }
  });

  const req = client.connect({
    address: 'localhost',
    port: server.address.port,
    servername: kServerName,
  });

  req.on('secure', common.mustCall((servername, alpn, cipher) => {
    debug('QuicClientSession TLS Handshake Complete');

    const stream = req.openStream();

    // TODO(@jasnell): The close happens synchronously, before any
    // data for the stream is actually flushed our to the connected
    // peer.
    stream.write('hello', common.mustCall());
    stream.close(1);

    // The abort event should emit because the stream closed abruptly
    // before the stream was finished.
    stream.on('abort', common.mustCall((code, finalSize) => {
      debug('Bidirectional, Client-initated stream %d aborted', stream.id);
      assert.strictEqual(code, 1);
      countdown.dec();
    }));

    stream.on('end', common.mustCall(() => {
      debug('Bidirectional, Client-initiated stream %d ended on client',
            stream.id);
    }));

    stream.on('close', common.mustCall(() => {
      debug('Bidirectional, Client-initiated stream %d closed on client',
            stream.id);
    }));

    debug('Bidirectional, Client-initiated stream %d opened', stream.id);
  }));

  req.on('stream', common.mustCall((stream) => {
    debug('Unidirectional, Server-initiated stream %d received', stream.id);
    stream.on('abort', common.mustNotCall());
    stream.on('data', common.mustCall((chunk) => {
      assert.strictEqual(chunk.toString(), 'hi');
    }));
    stream.on('end', common.mustCall(() => {
      debug('Unidirectional, Server-initiated stream %d ended on client',
            stream.id);
    }));
    stream.on('close', common.mustCall(() => {
      debug('Unidirectional, Server-initiated stream %d closed on client',
            stream.id);
      countdown.dec();
    }));
  }));

  req.on('close', common.mustCall());
}));

server.on('listening', common.mustCall());
server.on('close', common.mustCall());
