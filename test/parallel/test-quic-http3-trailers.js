// Flags: --expose-internals --no-warnings
'use strict';

// Tests a simple QUIC HTTP/3 client/server round-trip

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const Countdown = require('../common/countdown');
const assert = require('assert');
const { key, cert, ca, kHttp3Alpn } = require('../common/quic');

const { createQuicSocket } = require('net');

const options = { key, cert, ca, alpn: kHttp3Alpn };
const client = createQuicSocket({ client: options });
const server = createQuicSocket({ server: options });

client.on('close', common.mustCall());
server.on('close', common.mustCall());

const countdown = new Countdown(1, () => {
  server.close();
  client.close();
});

(async function() {
  server.on('session', common.mustCall((session) => {

    session.on('stream', common.mustCall((stream) => {
      assert(stream.submitInitialHeaders({ ':status': '200' }));

      stream.submitTrailingHeaders({ 'a': 1 });
      stream.end('hello world');
      stream.resume();
      stream.on('end', common.mustCall());
      stream.on('close', common.mustCall());
      stream.on('finish', common.mustCall());

      stream.on('initialHeaders', common.mustCall((headers) => {
        const expected = [
          [ ':path', '/' ],
          [ ':authority', 'localhost' ],
          [ ':scheme', 'https' ],
          [ ':method', 'POST' ]
        ];
        assert.deepStrictEqual(expected, headers);
      }));

      stream.on('trailingHeaders', common.mustCall((headers) => {
        const expected = [ [ 'b', '2' ] ];
        assert.deepStrictEqual(expected, headers);
      }));

      stream.on('informationalHeaders', common.mustNotCall());
    }));

    session.on('close', common.mustCall());
  }));

  await server.listen();

  const req = await client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port,
    maxStreamsUni: 10,
    h3: { maxPushes: 10 }
  });

  req.on('close', common.mustCall());

  const stream = await req.openStream();

  stream.on('trailingHeaders', common.mustCall((headers) => {
    const expected = [ [ 'a', '1' ] ];
    assert.deepStrictEqual(expected, headers);
  }));

  assert(stream.submitInitialHeaders({
    ':method': 'POST',
    ':scheme': 'https',
    ':authority': 'localhost',
    ':path': '/',
  }));

  stream.submitTrailingHeaders({ 'b': 2 });
  stream.end('hello world');
  stream.resume();
  stream.on('finish', common.mustCall());
  stream.on('end', common.mustCall());

  stream.on('initialHeaders', common.mustCall((headers) => {
    const expected = [
      [ ':status', '200' ]
    ];
    assert.deepStrictEqual(expected, headers);
  }));
  stream.on('informationalHeaders', common.mustNotCall());

  stream.on('close', common.mustCall(() => {
    countdown.dec();
  }));
})().then(common.mustCall());
