'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that the built-in HTTP/2 diagnostics channels are reporting
// the diagnostics messages for the 'http2.server.stream.finish' channel when
// ServerHttp2Streams#respond() sends a regular stream as well as a push stream.

const Countdown = require('../common/countdown');
const assert = require('assert');
const dc = require('diagnostics_channel');
const http2 = require('http2');
const { Duplex } = require('stream');

const serverHttp2StreamFinishCount = 2;

dc.subscribe('http2.server.stream.finish', common.mustCall(({ stream, headers, flags }) => {
  // Since ServerHttp2Stream is not exported from any module, this just checks
  // if the stream is an instance of Duplex and the constructor name is
  // 'ServerHttp2Stream'.
  assert.ok(stream instanceof Duplex);
  assert.strictEqual(stream.constructor.name, 'ServerHttp2Stream');

  assert.ok(headers && !Array.isArray(headers) && typeof headers === 'object');

  assert.strictEqual(typeof flags, 'number');
}, serverHttp2StreamFinishCount));

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

  const countdown = new Countdown(serverHttp2StreamFinishCount, () => {
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
