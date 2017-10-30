'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

// we use the lower-level API here
server.on('stream', common.mustNotCall());
server.listen(0);

server.on('listening', common.mustCall(() => {

  // Setting the maxSendHeaderBlockLength, then attempting to send a
  // headers block that is too big should cause a 'frameError' to
  // be emitted, and will cause the stream to be shutdown.
  const options = {
    maxSendHeaderBlockLength: 10
  };

  const client = h2.connect(`http://localhost:${server.address().port}`,
                            options);

  const req = client.request({ ':path': '/' });

  req.on('response', common.mustNotCall());

  req.resume();
  req.on('end', common.mustCall(() => {
    client.destroy();
  }));

  req.on('frameError', common.mustCall((type, code) => {
    assert.strictEqual(code, h2.constants.NGHTTP2_ERR_FRAME_SIZE_ERROR);
  }));

  req.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    type: Error,
    message: 'Stream closed with error code 7'
  }));

  req.end();

  // if no frameError listener, should emit 'error' with
  // code ERR_HTTP2_FRAME_ERROR
  const req2 = client.request({ ':path': '/' });

  req2.on('response', common.mustNotCall());

  req2.resume();
  req2.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));

  req2.once('error', common.mustCall((err) => {
    common.expectsError({
      code: 'ERR_HTTP2_FRAME_ERROR',
      type: Error
    })(err);
    req2.on('error', common.expectsError({
      code: 'ERR_HTTP2_STREAM_ERROR',
      type: Error,
      message: 'Stream closed with error code 7'
    }));
  }));

  req2.end();

}));
