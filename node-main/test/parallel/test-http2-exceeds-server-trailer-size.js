'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { createServer, constants, connect } = require('http2');

const server = createServer();

server.on('stream', (stream, headers) => {
  stream.respond(undefined, { waitForTrailers: true });

  stream.on('data', common.mustNotCall());

  stream.on('wantTrailers', common.mustCall(() => {
    // Trigger a frame error by sending a trailer that is too large
    stream.sendTrailers({ 'test-trailer': 'X'.repeat(64 * 1024) });
  }));

  stream.on('frameError', common.mustCall((frameType, errorCode) => {
    assert.strictEqual(errorCode, constants.NGHTTP2_FRAME_SIZE_ERROR);
  }));

  stream.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
  }));

  stream.on('close', common.mustCall());

  stream.end();
});

server.listen(0, () => {
  const clientSession = connect(`http://localhost:${server.address().port}`);

  clientSession.on('frameError', common.mustNotCall());
  clientSession.on('close', common.mustCall(() => {
    server.close();
  }));

  const clientStream = clientSession.request();

  clientStream.on('close', common.mustCall());
  clientStream.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    name: 'Error',
    message: 'Stream closed with error code NGHTTP2_FRAME_SIZE_ERROR'
  }));
  // This event mustn't be called once the frame size error is from the server
  clientStream.on('frameError', common.mustNotCall());

  clientStream.end();
});
