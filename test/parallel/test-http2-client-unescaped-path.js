// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

const server = http2.createServer();

server.on('stream', common.mustNotCall());

const count = 32;

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  let remaining = count + 1;
  function maybeClose() {
    if (--remaining === 0) {
      server.close();
      client.destroy();
    }
  }

  // nghttp2 will catch the bad header value for us.
  function doTest(i) {
    const req = client.request({ ':path': `bad${String.fromCharCode(i)}path` });
    req.on('error', common.expectsError({
      code: 'ERR_HTTP2_STREAM_ERROR',
      type: Error,
      message: 'Stream closed with error code 1'
    }));
    req.on('streamClosed', common.mustCall(maybeClose));
  }

  for (let i = 0; i <= count; i += 1)
    doTest(i);
}));
