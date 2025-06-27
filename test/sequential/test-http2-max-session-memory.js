'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');

// Test that maxSessionMemory Caps work

const largeBuffer = Buffer.alloc(2e6);

const server = http2.createServer({ maxSessionMemory: 1 });

server.on('stream', common.mustCall((stream) => {
  stream.on('error', (err) => {
    if (err.code !== 'ECONNRESET')
      throw err;
  });
  stream.respond();
  stream.end(largeBuffer);
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  {
    const req = client.request();

    req.on('response', () => {
      // This one should be rejected because the server is over budget
      // on the current memory allocation
      const req = client.request();
      req.on('error', common.expectsError({
        code: 'ERR_HTTP2_STREAM_ERROR',
        name: 'Error',
        message: 'Stream closed with error code NGHTTP2_ENHANCE_YOUR_CALM'
      }));
      req.on('close', common.mustCall(() => {
        server.close();
        client.destroy();
      }));
    });

    req.resume();
    req.on('close', common.mustCall());
  }
}));
