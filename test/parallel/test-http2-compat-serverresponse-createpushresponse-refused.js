'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// A client refusing a push created through the compat API with
// RST_STREAM(REFUSED_STREAM) must not crash the server with an unhandled
// 'error' event on the pushed stream.

const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer(common.mustCall((req, res) => {
  res.createPushResponse({ ':path': '/style.css' }, common.mustSucceed((pushRes) => {
    pushRes.on('close', common.mustCall());
    pushRes.setHeader('content-type', 'text/css');
    pushRes.end('body { color: red }');
  }));
  res.end('hello');
}));

server.listen(0, common.mustCall(() => {
  // A zero initial window keeps the pushed body from completing before the
  // client refuses it, so the server sees a peer reset with its writable
  // side still pending.
  const client = http2.connect(`http://localhost:${server.address().port}`, {
    settings: { initialWindowSize: 0 },
  });

  client.on('stream', common.mustCall((pushed) => {
    pushed.on('error', common.expectsError({
      code: 'ERR_HTTP2_STREAM_ERROR',
    }));
    pushed.close(http2.constants.NGHTTP2_REFUSED_STREAM);
    // Reopen the window so the main response can complete.
    client.settings({ initialWindowSize: 65535 });
  }));

  const req = client.request({ ':path': '/' });
  req.resume();
  req.on('close', common.mustCall(() => {
    assert.strictEqual(req.rstCode, http2.constants.NGHTTP2_NO_ERROR);
    client.close();
    server.close();
  }));
  req.end();
}));
