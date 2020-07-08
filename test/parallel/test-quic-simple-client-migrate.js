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
} = require('../common/quic');

const { createQuicSocket } = require('net');
const { pipeline } = require('stream');

let req;
let client;
let client2;
const server = createQuicSocket();

const options = { key, cert, ca, alpn: 'zzz' };
const countdown = new Countdown(2, () => {
  debug('Countdown expired. Destroying sockets');
  req.close();
  server.close();
  client2.close();
});

server.listen(options);

server.on('session', common.mustCall((session) => {
  debug('QuicServerSession Created');

  session.on('stream', common.mustCall((stream) => {
    debug('Bidirectional, Client-initiated stream %d received', stream.id);
    pipeline(stream, stream, common.mustCall());

    session.openStream({ halfOpen: true }).end('Hello from the server');
  }));

}));

server.on('ready', common.mustCall(() => {
  debug('Server is listening on port %d', server.endpoints[0].address.port);

  client = createQuicSocket({ client: options });
  client2 = createQuicSocket({ client: options });

  req = client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
  });

  client.on('close', common.mustCall());

  req.on('secure', common.mustCall(() => {
    debug('QuicClientSession TLS Handshake Complete');

    let data = '';

    const stream = req.openStream();
    debug('Bidirectional, Client-initiated stream %d opened', stream.id);
    stream.setEncoding('utf8');
    stream.on('data', (chunk) => data += chunk);
    stream.on('end', common.mustCall(() => {
      assert.strictEqual(data, 'Hello from the client');
      debug('Client received expected data for stream %d', stream.id);
    }));
    stream.on('close', common.mustCall(() => {
      debug('Bidirectional, Client-initiated stream %d closed', stream.id);
      countdown.dec();
    }));
    // Send some data on one connection...
    stream.write('Hello ');

    // Wait just a bit, then migrate to a different
    // QuicSocket and continue sending.
    setTimeout(common.mustCall(() => {
      req.setSocket(client2, (err) => {
        assert(!err);
        client.close();
        stream.end('from the client');
      });
    }), common.platformTimeout(100));
  }));

  req.on('stream', common.mustCall((stream) => {
    debug('Unidirectional, Server-initiated stream %d received', stream.id);
    let data = '';
    stream.setEncoding('utf8');
    stream.on('data', (chunk) => data += chunk);
    stream.on('end', common.mustCall(() => {
      assert.strictEqual(data, 'Hello from the server');
      debug('Client received expected data for stream %d', stream.id);
    }));
    stream.on('close', common.mustCall(() => {
      debug('Unidirectional, Server-initiated stream %d closed', stream.id);
      countdown.dec();
    }));
  }));
}));

server.on('listening', common.mustCall());
server.on('close', common.mustCall());
