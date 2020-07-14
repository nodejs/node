// Flags: --expose-internals --no-warnings
'use strict';

// Tests a simple QUIC HTTP/3 client/server round-trip

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const Countdown = require('../common/countdown');
const assert = require('assert');
const fs = require('fs');

const {
  key,
  cert,
  ca,
  debug,
  kHttp3Alpn,
  kServerPort,
  kClientPort,
  setupKeylog,
} = require('../common/quic');

const filedata = fs.readFileSync(__filename, { encoding: 'utf8' });

const { createQuicSocket } = require('net');
const kServerName = 'agent2';  // Intentionally the wrong servername

const options = { key, cert, ca, alpn: kHttp3Alpn };

const client = createQuicSocket({
  endpoint: { port: kClientPort },
  client: options
});
const server = createQuicSocket({
  endpoint: { port: kServerPort },
  server: options
});

client.on('close', common.mustCall());
server.on('close', common.mustCall());

const countdown = new Countdown(1, () => {
  debug('Countdown expired. Destroying sockets');
  server.close();
  client.close();
});

(async function() {
  server.on('session', common.mustCall((session) => {
    debug('QuicServerSession Created');

    assert.strictEqual(session.maxStreams.bidi, 100);
    assert.strictEqual(session.maxStreams.uni, 3);

    setupKeylog(session);

    session.on('secure', common.mustCall((_, alpn) => {
      debug('QuicServerSession handshake completed');
      assert.strictEqual(session.alpnProtocol, alpn);
    }));

    session.on('stream', common.mustCall((stream) => {
      debug('Bidirectional, Client-initiated stream %d received', stream.id);
      const file = fs.createReadStream(__filename);
      let data = '';

      assert(stream.submitInitialHeaders({ ':status': '200' }));

      file.pipe(stream);
      stream.setEncoding('utf8');

      stream.on('initialHeaders', common.mustCall((headers) => {
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

  await server.listen();

  debug('Server is listening on port %d', server.endpoints[0].address.port);

  const req = await client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port,
    servername: kServerName,
    h3: { maxPushes: 10 }
  });
  debug('QuicClientSession Created');

  req.on('close', common.mustCall());

  const file = fs.createReadStream(__filename);
  const stream = await req.openStream();

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

})().then(common.mustCall());
