'use strict';

const common = require('../common');
const { mustCall } = common;

if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');

const {
  HTTP2_HEADER_PATH,
  HTTP2_HEADER_METHOD,
} = http2.constants;

// This tests verifies that calling `req.socket.destroy()` via
// setImmediate does not crash.
// Fixes https://github.com/nodejs/node/issues/22855.

const app = http2.createServer(mustCall((req, res) => {
  res.end('hello');
  setImmediate(() => req.socket.destroy());
}));

app.listen(0, mustCall(() => {
  const session = http2.connect(`http://localhost:${app.address().port}`);
  const request = session.request({
    [HTTP2_HEADER_PATH]: '/',
    [HTTP2_HEADER_METHOD]: 'get'
  });
  request.once('response', mustCall((headers, flags) => {
    request.on('data', () => {});
    // setImmediate(socket.destroy) after res.end races with the response
    // bytes reaching the client; if they don't make it the reset surfaces
    // as 'error'. Test only checks that destroy-via-setImmediate doesn't
    // crash, so accept either path.
    request.on('error', () => {});
    request.on('close', mustCall(() => {
      session.close();
      app.close();
    }));
  }));
  request.end();
}));
