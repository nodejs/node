'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Regression test for https://github.com/nodejs/node/issues/56627
// When a client stream's writable side is already ended (e.g. GET request)
// and the server destroys the session, the client stream should emit an
// error event with RST code NGHTTP2_CANCEL, since the 'aborted' event
// cannot be emitted when the writable side is already ended.

{
  const server = h2.createServer();
  server.on('stream', common.mustCall((stream) => {
    stream.session.destroy();
  }));

  server.listen(0, common.mustCall(() => {
    const client = h2.connect(`http://localhost:${server.address().port}`);

    client.on('close', common.mustCall(() => {
      server.close();
    }));

    const req = client.request();

    req.on('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ERR_HTTP2_STREAM_ERROR');
      assert.match(err.message, /NGHTTP2_CANCEL/);
    }));

    req.on('aborted', common.mustNotCall());

    req.on('close', common.mustCall(() => {
      assert.strictEqual(req.rstCode, h2.constants.NGHTTP2_CANCEL);
    }));

    req.resume();
  }));
}
