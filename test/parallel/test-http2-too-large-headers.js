'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const assert = require('assert');
const {
  NGHTTP2_ENHANCE_YOUR_CALM
} = http2.constants;

const server = http2.createServer({ settings: { maxHeaderListSize: 100 } });
server.on('stream', common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  client.on('remoteSettings', () => {
    const req = client.request({ 'foo': 'a'.repeat(1000) });
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
  });

}));
