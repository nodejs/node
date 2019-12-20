// Flags: --expose-internals
'use strict';

// Tests a simple QUIC HTTP/3 client/server round-trip

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const Countdown = require('../common/countdown');
const assert = require('assert');
const fs = require('fs');
const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');
const { debuglog } = require('util');
const debug = debuglog('test');

const filedata = fs.readFileSync(__filename, { encoding: 'utf8' });

const { createSocket } = require('quic');

const kServerPort = process.env.NODE_DEBUG_KEYLOG ? 5678 : 0;
const kClientPort = process.env.NODE_DEBUG_KEYLOG ? 5679 : 0;

let client;
const server = createSocket({
  endpoint: { port: kServerPort },
  validateAddress: true
});

const kServerName = 'agent2';  // Intentionally the wrong servername
const kALPN = 'h3-24';

const countdown = new Countdown(1, () => {
  debug('Countdown expired. Destroying sockets');
  server.close();
  client.close();
});

server.listen({
  key,
  cert,
  ca,
  requestCert: true,
  rejectUnauthorized: false,
  alpn: kALPN,
  maxCryptoBuffer: 4096,
});
server.on('session', common.mustCall((session) => {
  debug('QuicServerSession Created');

  assert.strictEqual(session.maxStreams.bidi, 100);
  assert.strictEqual(session.maxStreams.uni, 3);

  if (process.env.NODE_DEBUG_KEYLOG) {
    const kl = fs.createWriteStream(process.env.NODE_DEBUG_KEYLOG);
    session.on('keylog', kl.write.bind(kl));
  }

  session.on('secure', common.mustCall((servername, alpn) => {
    debug('QuicServerSession handshake completed');
    assert.strictEqual(session.alpnProtocol, alpn);
  }));

  session.on('stream', common.mustCall((stream) => {
    debug('Bidirectional, Client-initiated stream %d received', stream.id);
    const file = fs.createReadStream(__filename);
    let data = '';

    assert(stream.submitInitialHeaders({
      ':status': '200',
    }));

    file.pipe(stream);
    stream.setEncoding('utf8');

    stream.on('initialHeaders', common.mustCall((headers) => {
      // TODO(@jasnell): Update when headers to changed to an object
      const expected = [
        [ ':path', '/' ],
        [ ':authority', 'localhost' ],
        [ ':scheme', 'https' ],
        [ ':method', 'POST' ]
      ];
      assert.deepStrictEqual(expected, headers);
      debug('Received expected request headers');
    }));
    stream.on('informationalHeaders', common.mustNotCall());
    stream.on('trailingHeaders', common.mustNotCall());

    stream.on('data', (chunk) => {
      data += chunk;
    });
    stream.on('end', common.mustCall(() => {
      assert.strictEqual(data, filedata);
      debug('Server received expected data for stream %d', stream.id);
    }));
    stream.on('close', common.mustCall());
    stream.on('finish', common.mustCall());
  }));

  session.on('close', common.mustCall());
}));

server.on('ready', common.mustCall(() => {
  debug('Server is listening on port %d', server.endpoints[0].address.port);
  client = createSocket({
    endpoint: { port: kClientPort },
    client: {
      key,
      cert,
      ca,
      alpn: kALPN,
    }
  });

  client.on('close', common.mustCall());

  const req = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port,
    servername: kServerName,
    h3: { maxPushes: 10 }
  });
  debug('QuicClientSession Created');

  req.on('secure', common.mustCall((servername, alpn, cipher) => {
    debug('QuicClientSession handshake completed');

    const file = fs.createReadStream(__filename);
    const stream = req.openStream();

    assert(stream.submitInitialHeaders({
      ':method': 'POST',
      ':scheme': 'https',
      ':authority': 'localhost',
      ':path': '/',
    }));
    file.pipe(stream);
    let data = '';
    stream.resume();
    stream.setEncoding('utf8');

    stream.on('initialHeaders', common.mustCall((headers) => {
      // TODO(@jasnell): Update when headers to changed to an object
      const expected = [
        [ ':status', '200' ]
      ];
      assert.deepStrictEqual(expected, headers);
      debug('Received expected response headers');
    }));
    stream.on('informationalHeaders', common.mustNotCall());
    stream.on('trailingHeaders', common.mustNotCall());

    stream.on('data', (chunk) => data += chunk);
    stream.on('finish', common.mustCall());
    stream.on('end', common.mustCall(() => {
      assert.strictEqual(data, filedata);
      debug('Client received expected data for stream %d', stream.id);
    }));
    stream.on('close', common.mustCall(() => {
      debug('Bidirectional, Client-initiated stream %d closed', stream.id);
      countdown.dec();
    }));
    debug('Bidirectional, Client-initiated stream %d opened', stream.id);
  }));

  req.on('close', common.mustCall());
}));

server.on('listening', common.mustCall());
server.on('close', common.mustCall());
