// Flags: --expose-internals --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const Countdown = require('../common/countdown');
const assert = require('assert');
const {
  key,
  cert,
  ca,
  debug,
  kServerPort,
  kClientPort,
  setupKeylog
} = require('../common/quic');

const { createQuicSocket } = require('net');

let client;
const server = createQuicSocket({ endpoint: { port: kServerPort } });

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

  setupKeylog(session);

  session.on('secure', common.mustCall((servername, alpn, cipher) => {
    const uni = session.openStream({ halfOpen: true });

    uni.write('hi', common.expectsError());


    uni.on('error', common.mustCall(() => {
      assert.strictEqual(uni.aborted, true);
    }));

    uni.on('data', common.mustNotCall());
    uni.on('close', common.mustCall(() => {
      debug('Unidirectional, Server-initiated stream %d closed on server',
            uni.id);
    }));

    uni.close(3);
    debug('Unidirectional, Server-initiated stream %d opened', uni.id);
  }));

  session.on('stream', common.mustNotCall());
  session.on('close', common.mustCall());
}));

server.on('ready', common.mustCall(() => {
  debug('Server is listening on port %d', server.endpoints[0].address.port);
  client = createQuicSocket({
    endpoint: { port: kClientPort },
    client: { key, cert, ca, alpn: kALPN }
  });

  const req = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port,
    servername: kServerName,
  });

  req.on('secure', common.mustCall((servername, alpn, cipher) => {
    debug('QuicClientSession TLS Handshake Complete');

    const stream = req.openStream();

    stream.write('hello', common.expectsError());
    stream.write('there', common.expectsError());

    stream.on('error', common.mustCall(() => {
      assert.strictEqual(stream.aborted, true);
    }));

    stream.on('end', common.mustNotCall());

    stream.on('close', common.mustCall(() => {
      countdown.dec();
    }));

    stream.close(1);

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
