'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const assert = require('assert');
const {
  NGHTTP2_ENHANCE_YOUR_CALM
} = http2.constants;

// By default, the maximum number of header fields allowed per
// block is 1024, including the HTTP pseudo-header fields, by
// setting one here, no request should be acccepted because it
// does not allow for even the basic HTTP pseudo-headers
const server = http2.createServer({ maxHeaderListPairs: 1 });
server.on('stream', common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const req = client.request();
  req.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    type: Error,
    message: 'Stream closed with error code 11'
  }));
  req.on('streamClosed', common.mustCall((code) => {
    assert.strictEqual(code, NGHTTP2_ENHANCE_YOUR_CALM);
    server.close();
    client.destroy();
  }));

}));
