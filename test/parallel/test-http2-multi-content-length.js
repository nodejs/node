// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  stream.respond();
  stream.end();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  let remaining = 3;
  function maybeClose() {
    if (--remaining === 0) {
      server.close();
      client.destroy();
    }
  }

  {
    // Request 1 will fail because there are two content-length header values
    const req = client.request({
      ':method': 'POST',
      'content-length': 1,
      'Content-Length': 2
    });
    req.on('error', common.expectsError({
      code: 'ERR_HTTP2_HEADER_SINGLE_VALUE',
      type: Error,
      message: 'Header field "content-length" must have only a single value'
    }));
    req.on('error', common.mustCall(maybeClose));
    req.end('a');
  }

  {
    // Request 2 will succeed
    const req = client.request({
      ':method': 'POST',
      'content-length': 1
    });
    req.resume();
    req.on('end', common.mustCall(maybeClose));
    req.end('a');
  }

  {
    // Request 3 will fail because  nghttp2 does not allow the content-length
    // header to be set for non-payload bearing requests...
    const req = client.request({ 'content-length': 1 });
    req.resume();
    req.on('end', common.mustCall(maybeClose));
    req.on('error', common.expectsError({
      code: 'ERR_HTTP2_STREAM_ERROR',
      type: Error,
      message: 'Stream closed with error code 1'
    }));
  }
}));
