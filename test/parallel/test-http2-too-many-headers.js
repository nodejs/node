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
// block is 128, including the HTTP pseudo-header fields. The
// minimum value for servers is 4, setting this to any value
// less than 4 will still leave the minimum to 4.
const server = http2.createServer({ maxHeaderListPairs: 0 });
server.on('stream', common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const req = client.request({ foo: 'bar' });
  req.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    type: Error,
    message: 'Stream closed with error code NGHTTP2_ENHANCE_YOUR_CALM'
  }));
  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.rstCode, NGHTTP2_ENHANCE_YOUR_CALM);
    server.close();
    client.close();
  }));

}));
