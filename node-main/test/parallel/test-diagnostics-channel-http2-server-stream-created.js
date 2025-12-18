'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that the built-in HTTP/2 diagnostics channels are reporting
// the diagnostics messages for the 'http2.server.stream.created' channel when
// ServerHttp2Streams are created by both:
// - in response to an incoming 'stream' event from the client
// - the server calling ServerHttp2Stream#pushStream()

const Countdown = require('../common/countdown');
const assert = require('assert');
const dc = require('diagnostics_channel');
const http2 = require('http2');
const { Duplex } = require('stream');

const serverHttp2StreamCreationCount = 2;

dc.subscribe('http2.server.stream.created', common.mustCall(({ stream, headers }) => {
  // Since ServerHttp2Stream is not exported from any module, this just checks
  // if the stream is an instance of Duplex and the constructor name is
  // 'ServerHttp2Stream'.
  assert.ok(stream instanceof Duplex);
  assert.strictEqual(stream.constructor.name, 'ServerHttp2Stream');
  assert.ok(headers && !Array.isArray(headers) && typeof headers === 'object');
}, serverHttp2StreamCreationCount));

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.respond();
  stream.end();

  stream.pushStream({}, common.mustSucceed((pushStream) => {
    pushStream.respond();
    pushStream.end();
  }));
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);

  const countdown = new Countdown(serverHttp2StreamCreationCount, () => {
    client.close();
    server.close();
  });

  const stream = client.request({});
  stream.on('response', common.mustCall(() => {
    countdown.dec();
  }));

  client.on('stream', common.mustCall((pushStream) => {
    pushStream.on('push', common.mustCall(() => {
      countdown.dec();
    }));
  }));
}));
