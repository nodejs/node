// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

const server = h2.createServer();

// we use the lower-level API here
server.on('stream', common.mustCall((stream) => {
  stream.respond({ ':status': 200 });

  // The first pushStream will complete as normal
  stream.pushStream({
    ':scheme': 'http',
    ':path': '/foobar',
    ':authority': `localhost:${server.address().port}`,
  }, common.mustCall((pushedStream) => {
    pushedStream.respond({ ':status': 200 });
    pushedStream.end();
    pushedStream.on('aborted', common.mustNotCall());
  }));

  // The second pushStream will be aborted because the client
  // will reject it due to the maxReservedRemoteStreams option
  // being set to only 1
  stream.pushStream({
    ':scheme': 'http',
    ':path': '/foobar',
    ':authority': `localhost:${server.address().port}`,
  }, common.mustCall((pushedStream) => {
    pushedStream.respond({ ':status': 200 });
    pushedStream.on('aborted', common.mustCall());
    pushedStream.on('error', common.mustCall(common.expectsError({
      code: 'ERR_HTTP2_STREAM_ERROR',
      type: Error,
      message: 'Stream closed with error code 8'
    })));
  }));

  stream.end('hello world');
}));
server.listen(0);

server.on('listening', common.mustCall(() => {

  const options = {
    maxReservedRemoteStreams: 1
  };

  const client = h2.connect(`http://localhost:${server.address().port}`,
                            options);

  let remaining = 2;
  function maybeClose() {
    if (--remaining === 0) {
      server.close();
      client.destroy();
    }
  }

  const req = client.request({ ':path': '/' });

  // Because maxReservedRemoteStream is 1, the stream event
  // must only be emitted once, even tho the server sends
  // two push streams.
  client.on('stream', common.mustCall((stream) => {
    stream.resume();
    stream.on('end', common.mustCall());
    stream.on('streamClosed', common.mustCall(maybeClose));
  }));

  req.on('response', common.mustCall());

  req.resume();
  req.on('end', common.mustCall());
  req.on('streamClosed', common.mustCall(maybeClose));
}));
