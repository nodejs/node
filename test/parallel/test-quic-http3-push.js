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
const { createWriteStream } = require('fs');

const qlog = process.env.NODE_QLOG === '1';

const options = { key, cert, ca, alpn: kHttp3Alpn, qlog };

const client = createQuicSocket({ qlog, client: options });
const server = createQuicSocket({ qlog, server: options });

client.on('close', common.mustCall());
server.on('close', common.mustCall());

const countdown = new Countdown(2, () => {
  server.close();
  client.close();
});

(async function() {
  server.on('session', common.mustCall((session) => {
    if (qlog) session.qlog.pipe(createWriteStream('server.qlog'));
    session.on('stream', common.mustCall((stream) => {
      assert(stream.submitInitialHeaders({ ':status': '200' }));

      [-1, Number.MAX_SAFE_INTEGER + 1].forEach((highWaterMark) => {
        assert.throws(() => stream.pushStream({}, { highWaterMark }), {
          code: 'ERR_OUT_OF_RANGE'
        });
      });
      ['', 1n, {}, [], false].forEach((highWaterMark) => {
        assert.throws(() => stream.pushStream({}, { highWaterMark }), {
          code: 'ERR_INVALID_ARG_TYPE'
        });
      });
      ['', 1, 1n, true, [], {}, 'zebra'].forEach((defaultEncoding) => {
        assert.throws(() => stream.pushStream({}, { defaultEncoding }), {
          code: 'ERR_INVALID_ARG_VALUE'
        });
      });

      const push = stream.pushStream({
        ':method': 'GET',
        ':scheme': 'https',
        ':authority': 'localhost',
        ':path': '/foo'
      });
      assert(push);
      push.submitInitialHeaders({ ':status': '200' });
      push.end('testing');
      push.on('close', common.mustCall());
      // TODO(@jasnell): There's current a bug where the last
      // _final callback is not being invoked in this case
      // push.on('finish', common.mustCall());

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
      stream.on('informationalHeaders', common.mustNotCall());
      stream.on('trailingHeaders', common.mustNotCall());
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
  if (qlog) req.qlog.pipe(createWriteStream('client.qlog'));

  req.on('stream', common.mustCall((stream) => {
    let data = '';

    stream.on('initialHeaders', common.mustCall((headers) => {
      const expected = [
        [':status', '200']
      ];
      assert.deepStrictEqual(expected, headers);
    }));

    stream.setEncoding('utf8');
    stream.on('data', (chunk) => data += chunk);
    stream.on('end', common.mustCall(() => {
      assert.strictEqual(data, 'testing');
    }));
    stream.on('close', common.mustCall(() => {
      countdown.dec();
    }));
  }));

  req.on('close', common.mustCall());

  const stream = await req.openStream();

  stream.on('pushHeaders', common.mustCall((headers, push_id) => {
    const expected = [
      [ ':path', '/foo' ],
      [ ':authority', 'localhost' ],
      [ ':scheme', 'https' ],
      [ ':method', 'GET' ]
    ];
    assert.deepStrictEqual(expected, headers);
    assert.strictEqual(push_id, 0);
  }));

  stream.on('initialHeaders', common.mustCall((headers) => {
    const expected = [
      [ ':status', '200' ]
    ];
    assert.deepStrictEqual(expected, headers);
  }));
  stream.on('informationalHeaders', common.mustNotCall());
  stream.on('trailingHeaders', common.mustNotCall());

  stream.on('close', common.mustCall(() => {
    countdown.dec();
  }));

  assert(stream.submitInitialHeaders({
    ':method': 'POST',
    ':scheme': 'https',
    ':authority': 'localhost',
    ':path': '/',
  }));

  stream.end('hello world');
  stream.resume();
  stream.on('finish', common.mustCall());
  stream.on('end', common.mustCall());

})().then(common.mustCall());
