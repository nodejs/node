'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that the built-in HTTP/2 diagnostics channels are reporting
// the diagnostics messages for the 'http2.client.stream.created' channel when
// ClientHttp2Streams are created by both:
// - the client calling ClientHttp2Session#request()
// - in response to an incoming 'push' event from the server

const Countdown = require('../common/countdown');
const assert = require('assert');
const dc = require('diagnostics_channel');
const http2 = require('http2');
const { Duplex } = require('stream');

const clientHttp2StreamCreationCount = 2;

let countdown;
let port;

dc.subscribe('http2.client.stream.created', common.mustCall(({ stream, headers }) => {
  // Since ClientHttp2Stream is not exported from any module, this just checks
  // if the stream is an instance of Duplex and the constructor name is
  // 'ClientHttp2Stream'.
  assert.ok(stream instanceof Duplex);
  assert.strictEqual(stream.constructor.name, 'ClientHttp2Stream');
  assert.ok(headers && !Array.isArray(headers) && typeof headers === 'object');
  if (countdown.remaining === clientHttp2StreamCreationCount) {
    // The request stream headers.
    assert.deepStrictEqual(headers, {
      '__proto__': null,
      ':method': 'GET',
      ':authority': `localhost:${port}`,
      ':scheme': 'http',
      ':path': '/',
      'requestHeader': 'requestValue',
    });
  } else {
    // The push stream headers.
    assert.deepStrictEqual(headers, {
      '__proto__': null,
      ':method': 'GET',
      ':authority': `localhost:${port}`,
      ':scheme': 'http',
      ':path': '/',
      [http2.sensitiveHeaders]: [],
      'pushheader': 'pushValue',
    });
  }
}, clientHttp2StreamCreationCount));

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.respond();
  stream.end();

  stream.pushStream({ 'pushHeader': 'pushValue' }, common.mustSucceed((pushStream) => {
    pushStream.respond();
    pushStream.end();
  }, 1));
}, 1));

server.listen(0, common.mustCall(() => {
  port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);

  countdown = new Countdown(clientHttp2StreamCreationCount, () => {
    client.close();
    server.close();
  });

  const stream = client.request(['requestHeader', 'requestValue']);
  stream.on('response', common.mustCall(() => {
    countdown.dec();
  }));

  client.on('stream', common.mustCall((pushStream) => {
    pushStream.on('push', common.mustCall(() => {
      countdown.dec();
    }, 1));
  }, 1));
}, 1));
