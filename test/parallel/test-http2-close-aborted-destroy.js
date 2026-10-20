'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const { NGHTTP2_CANCEL } = http2.constants;

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  stream.respond();
  stream.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ABORTED',
  }));
  stream.on('close', common.mustCall(() => {
    assert.strictEqual(stream.rstCode, NGHTTP2_CANCEL);
    server.close();
  }));
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request({ ':method': 'POST' });

  req.on('aborted', common.mustCall(() => req.destroy()));
  req.on('close', common.mustCall(() => client.close()));
  req.on('response', common.mustCall(() => {
    req.close(NGHTTP2_CANCEL);
  }));
}));
