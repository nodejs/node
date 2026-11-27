'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const timeout = common.platformTimeout(20);

// When a request times out and no 'timeout' handler is attached to either the
// request or the response, the HTTP/2 compat layer must destroy the stream,
// matching the HTTP/1 behavior where the underlying socket is destroyed. This
// prevents a response from being sent after the timeout.
{
  const server = http2.createServer();
  let client;

  server.on('request', common.mustCall((req, res) => {
    // No 'timeout' listener is added anywhere.
    req.setTimeout(timeout);
    req.resume();

    // By the time this fires, the stream timeout has already elapsed, so the
    // stream must have been destroyed by the compat layer.
    setTimeout(common.mustCall(() => {
      assert.strictEqual(req.stream.destroyed, true);
      server.close();
      client.destroy();
    }), timeout * 3);
  }));

  server.listen(0, common.mustCall(() => {
    client = http2.connect(`http://localhost:${server.address().port}`);
    client.on('error', () => {});
    const req = client.request({ ':path': '/' });
    req.on('error', () => {});
    req.on('data', () => {});
    req.end();
  }));
}
