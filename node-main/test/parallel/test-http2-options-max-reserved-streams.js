'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
const Countdown = require('../common/countdown');

const server = h2.createServer();
let client;

const countdown = new Countdown(3, () => {
  server.close();
  client.close();
});

// We use the lower-level API here
server.on('stream', common.mustCall((stream) => {
  // The first pushStream will complete as normal
  stream.pushStream({
    ':path': '/foobar',
  }, common.mustSucceed((pushedStream) => {
    pushedStream.respond();
    pushedStream.end();
    pushedStream.on('aborted', common.mustNotCall());
  }));

  // The second pushStream will be aborted because the client
  // will reject it due to the maxReservedRemoteStreams option
  // being set to only 1
  stream.pushStream({
    ':path': '/foobar',
  }, common.mustSucceed((pushedStream) => {
    pushedStream.respond();
    pushedStream.on('aborted', common.mustCall());
    pushedStream.on('error', common.mustNotCall());
    pushedStream.on('close', common.mustCall(() => {
      assert.strictEqual(pushedStream.rstCode, 8);
      countdown.dec();
    }));
  }));

  stream.respond();
  stream.end('hello world');
}));
server.listen(0);

server.on('listening', common.mustCall(() => {
  client = h2.connect(`http://localhost:${server.address().port}`,
                      { maxReservedRemoteStreams: 1 });

  const req = client.request();

  // Because maxReservedRemoteStream is 1, the stream event
  // must only be emitted once, even tho the server sends
  // two push streams.
  client.on('stream', common.mustCall((stream) => {
    stream.resume();
    stream.on('push', common.mustCall());
    stream.on('end', common.mustCall());
    stream.on('close', common.mustCall(() => countdown.dec()));
  }));

  req.on('response', common.mustCall());
  req.resume();
  req.on('end', common.mustCall());
  req.on('close', common.mustCall(() => countdown.dec()));
}));
