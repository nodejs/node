'use strict';

// Destroying a stream that was closed with NO_ERROR while its response body
// was still queued behind flow control must still send RST_STREAM.
// Refs: https://github.com/nodejs/node/issues/67363

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const { NGHTTP2_NO_ERROR } = http2.constants;

const server = http2.createServer();

let serverStream;

server.on('stream', common.mustCall((stream) => {
  serverStream = stream;
  stream.on('error', common.mustNotCall());
  stream.on('close', common.mustCall(() => {
    assert.strictEqual(stream.rstCode, NGHTTP2_NO_ERROR);
  }));
  stream.respond();
  stream.end(Buffer.alloc(1024 * 1024));
}));

server.listen(0, common.mustCall(() => {
  // A small stream window makes sure that most of the body stays queued.
  const client = http2.connect(`http://localhost:${server.address().port}`,
                               { settings: { initialWindowSize: 16384 } });
  const req = client.request();
  req.on('response', common.mustCall(() => {
    // The response is not being read yet, so the response body is stuck
    // behind flow control when the server closes and destroys the stream.
    serverStream.close();
    serverStream.destroy();
    // Open the window again. Without the RST_STREAM this is what makes the
    // server loop.
    req.resume();
  }));
  // On this release line a clean reset before the body ends closes the
  // stream without an error.
  req.on('error', common.mustNotCall());
  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.rstCode, NGHTTP2_NO_ERROR);
    client.close();
    server.close();
  }));
  req.end();
}));
