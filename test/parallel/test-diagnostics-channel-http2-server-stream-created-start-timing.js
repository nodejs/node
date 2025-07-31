'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// This test ensures that the built-in HTTP/2 diagnostics channels are reporting
// the diagnostics messages for the 'http2.server.stream.start' channel only
// after the 'http2.server.stream.created' channel.

const Countdown = require('../common/countdown');
const assert = require('assert');
const dc = require('diagnostics_channel');
const http2 = require('http2');

const serverHttp2StreamCreationCount = 2;

const map = {};

dc.subscribe('http2.server.stream.created', common.mustCall(({ stream, headers }) => {
  map[stream.id] = { ...map[stream.id], 'createdTime': process.hrtime.bigint() };
}, serverHttp2StreamCreationCount));

dc.subscribe('http2.server.stream.start', common.mustCall(({ stream, headers }) => {
  map[stream.id] = { ...map[stream.id], 'startTime': process.hrtime.bigint() };
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

    const timings = Object.values(map);
    assert.strictEqual(timings.length, serverHttp2StreamCreationCount);

    for (const { createdTime, startTime } of timings) {
      assert.ok(createdTime < startTime);
    }
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
