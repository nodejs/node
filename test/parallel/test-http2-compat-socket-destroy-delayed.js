'use strict';

const common = require('../common');
const { mustCall, mustNotCall } = common;

if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');
const assert = require('assert');

// This tests verifies that calling `req.socket.destroy()` via
// setImmediate does not crash.
// Fixes https://github.com/nodejs/node/issues/22855.

const app = http2.createServer(mustCall((req, res) => {
  res.end('hello');
  setImmediate(() => req.socket.destroy());
}));

app.listen(0, mustCall(() => {
  const session = http2.connect(`http://localhost:${app.address().port}`);
  const request = session.request();
  request.once('response', mustCall((headers, flags) => {
    let data = '';
    request.on('data', (chunk) => { data += chunk; });
    // This should error since the server fails to finish the stream
    request.on('error', mustCall((err) => {
      common.expectsError({
        code: 'ERR_HTTP2_STREAM_ERROR',
        name: 'Error',
        message: 'Stream closed with error code NGHTTP2_CANCEL'
      })(err);
      assert.strictEqual(data, 'hello');
      session.close();
      app.close();
    }));
    request.on('end', mustNotCall());
  }));
  request.end();
}));
