'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const assert = require('assert');
const {
  DEFAULT_SETTINGS_MAX_HEADER_LIST_SIZE,
  // NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE
} = http2.constants;

for (const headerSize of [
  DEFAULT_SETTINGS_MAX_HEADER_LIST_SIZE - 101, // pass
  DEFAULT_SETTINGS_MAX_HEADER_LIST_SIZE - 100, // fail
]) {
  const timeout = common.platformTimeout(50);
  const timer = setTimeout(() => assert.fail(`http2 client timedout when receiving a header of size ${headerSize}`), timeout);

  const server = http2.createServer((req, res) => {
    res.setHeader('foobar', 'a'.repeat(headerSize));
    res.end(common.mustCall());
  });
  server.on('error', common.mustNotCall());

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);

    client.on('remoteSettings', () => {
      const req = client.request({ ':path': '/' });
      // TODO, I don't yet know which error to expect
      // req.on('error',
      //   (error) => common.expectsError({
      //   code: 'ERR_HTTP2_STREAM_ERROR',
      //   name: 'Error',
      //   message:
      //   'Stream closed with error code NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE'
      // }));
      req.on('close', common.mustCall(() => {
        clearTimeout(timer);
        // TODO, I don't yet know which error code to expect
        // assert.strictEqual(
        //   req.rstCode,
        //   NGHTTP2_SETTINGS_MAX_HEADER_LIST_SIZE
        // );
        server.close();
        client.close();
      }));
    });
  }));
}
