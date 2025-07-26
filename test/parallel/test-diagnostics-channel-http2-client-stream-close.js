'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that the built-in HTTP/2 diagnostics channels are reporting
// the diagnostics messages for the 'http2.client.stream.close' channel when
// ClientHttp2Streams created by these actions are closed:
// - the client calling ClientHttp2Session#request()
// - in response to an incoming 'push' event from the server

const Countdown = require('../common/countdown');
const assert = require('assert');
const dc = require('diagnostics_channel');
const http2 = require('http2');
const { Duplex } = require('stream');

const clientHttp2StreamCloseCount = 2;

dc.subscribe('http2.client.stream.close', common.mustCall(({ stream }) => {
  // Since ClientHttp2Stream is not exported from any module, this just checks
  // if the stream is an instance of Duplex and the constructor name is
  // 'ClientHttp2Stream'.
  assert.ok(stream instanceof Duplex);
  assert.strictEqual(stream.constructor.name, 'ClientHttp2Stream');
  assert.strictEqual(stream.closed, true);
  assert.strictEqual(stream.destroyed, false);

  assert.strictEqual(stream.rstCode, http2.constants.NGHTTP2_NO_ERROR);
}, clientHttp2StreamCloseCount));

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

  const countdown = new Countdown(clientHttp2StreamCloseCount, () => {
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
