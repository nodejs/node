'use strict';

// Destroying a stream that was closed with NO_ERROR while its request body
// was still queued behind flow control must still send RST_STREAM.
// Refs: https://github.com/nodejs/node/issues/67363

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const { NGHTTP2_NO_ERROR } = http2.constants;

// A small stream window makes sure that most of the body stays queued.
const server = http2.createServer({ settings: { initialWindowSize: 16384 } });

let req;

server.on('stream', common.mustCall((stream) => {
  // The stream is not being read yet, so the request body is stuck behind
  // flow control when the client closes and destroys the request.
  req.close();
  req.destroy();

  stream.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ABORTED',
  }));
  stream.on('close', common.mustCall(() => {
    assert.strictEqual(stream.rstCode, NGHTTP2_NO_ERROR);
    server.close();
  }));
  // Open the window again. Without the RST_STREAM this is what makes the
  // client loop.
  stream.resume();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  req = client.request({ ':method': 'POST' });
  req.on('error', common.mustNotCall());
  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.rstCode, NGHTTP2_NO_ERROR);
    client.close();
  }));
  req.end(Buffer.alloc(1024 * 1024));
}));
