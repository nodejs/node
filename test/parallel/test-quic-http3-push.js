// Flags: --expose-internals --no-warnings
'use strict';

// Tests a simple QUIC HTTP/3 client/server round-trip

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const Countdown = require('../common/countdown');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent1-key.pem', 'binary');
const cert = fixtures.readKey('agent1-cert.pem', 'binary');
const ca = fixtures.readKey('ca1-cert.pem', 'binary');

const { createSocket } = require('quic');

let client;
const server = createSocket();

const countdown = new Countdown(2, () => {
  server.close();
  client.close();
});

server.listen({ key, cert, ca, alpn: 'h3-24' });

server.on('session', common.mustCall((session) => {

  session.on('stream', common.mustCall((stream) => {
    assert(stream.submitInitialHeaders({ ':status': '200' }));

    const push = stream.pushStream({
      ':method': 'GET',
      ':scheme': 'https',
      ':authority': 'localhost',
      ':path': '/foo'
    });
    assert(push);
    push.submitInitialHeaders({ ':status': '200' });
    // TODO(@jasnell): There's currently a bug if we only call end() to write.
    // push.write('push ');
    push.write('test');
    push.end('ing');
    push.on('close', common.mustCall());
    push.on('finish', common.mustCall());

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

server.on('ready', common.mustCall(() => {
  client = createSocket({ client: { key, cert, ca, alpn: 'h3-24' } });
  client.on('close', common.mustCall());

  const req = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port,
    maxStreamsUni: 10,
    h3: { maxPushes: 10 }
  });

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
  req.on('secure', common.mustCall((servername, alpn, cipher) => {
    const stream = req.openStream();

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
  }));
}));

server.on('listening', common.mustCall());
server.on('close', common.mustCall());
