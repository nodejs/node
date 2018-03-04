'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

// we use the lower-level API here
server.on('stream', common.mustNotCall());
server.listen(0, common.mustCall(() => {

  // Setting the maxSendHeaderBlockLength, then attempting to send a
  // headers block that is too big should cause a 'frameError' to
  // be emitted, and will cause the stream to be shutdown.
  const options = {
    maxSendHeaderBlockLength: 10
  };

  const client = h2.connect(`http://localhost:${server.address().port}`,
                            options);

  const req = client.request();
  req.on('response', common.mustNotCall());

  req.resume();
  req.on('close', common.mustCall(() => {
    client.close();
    server.close();
  }));

  req.on('frameError', common.mustCall((type, code) => {
    assert.strictEqual(code, h2.constants.NGHTTP2_ERR_FRAME_SIZE_ERROR);
  }));

  req.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    type: Error,
    message: 'Stream closed with error code NGHTTP2_REFUSED_STREAM'
  }));
}));
