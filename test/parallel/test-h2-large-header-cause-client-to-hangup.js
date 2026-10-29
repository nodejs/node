'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const assert = require('assert');
const {
  DEFAULT_SETTINGS_MAX_HEADER_LIST_SIZE,
  NGHTTP2_FRAME_SIZE_ERROR,
} = http2.constants;

const headerSize = DEFAULT_SETTINGS_MAX_HEADER_LIST_SIZE;
const timeout = common.platformTimeout(2_000);
const timer = setTimeout(() => assert.fail(`http2 client timedout
when server can not manage to send a header of size ${headerSize}`), timeout);

const server = http2.createServer((req, res) => {
  res.setHeader('foobar', 'a'.repeat(DEFAULT_SETTINGS_MAX_HEADER_LIST_SIZE));
  res.end();
});

server.listen(0, common.mustCall(() => {
  const clientSession = http2.connect(`http://localhost:${server.address().port}`);
  clientSession.on('close', common.mustCall());
  clientSession.on('remoteSettings', send);

  function send() {
    const stream = clientSession.request({ ':path': '/' });
    stream.on('close', common.mustCall(() => {
      assert.strictEqual(stream.rstCode, NGHTTP2_FRAME_SIZE_ERROR);
      clearTimeout(timer);
      server.close();
    }));

    stream.on('error', common.expectsError({
      code: 'ERR_HTTP2_STREAM_ERROR',
      name: 'Error',
      message: 'Stream closed with error code NGHTTP2_FRAME_SIZE_ERROR'
    }));

    stream.end();
  }
}));
