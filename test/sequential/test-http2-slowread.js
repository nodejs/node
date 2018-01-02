'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const {
  NGHTTP2_FLOW_CONTROL_ERROR
} = http2.constants;


const largeChunk = Buffer.alloc(0xFFFFFF);

{
  const server = http2.createServer({ slowReadTimeout: 1000 });

  server.on('stream', (stream) => {
    stream.on('error', common.expectsError({
      code: 'ERR_HTTP2_SLOW_READ',
      type: Error,
      message: 'Connected peer failed to read data fast enough'
    }));
    stream.respond();
    stream.end(largeChunk);
  });

  server.listen(0, common.mustCall(() => {
    // Evil client creates a slow-read scenario, forcing our server to only
    // send one byte at a time.
    const client = http2.connect(`http://localhost:${server.address().port}`,
                                 { settings: { initialWindowSize: 1 } });

    const req = client.request();

    req.resume();
    req.on('close', () => {
      client.close();
      server.close();
    });

    req.on('error', common.expectsError({
      code: 'ERR_HTTP2_STREAM_ERROR',
      type: Error,
      message: `Stream closed with error code ${NGHTTP2_FLOW_CONTROL_ERROR}`
    }));
  }));
}


{
  const server = http2.createServer({ slowReadTimeout: 1000 });

  server.on('stream', (stream) => {
    // Handle the slow read event ourselves
    stream.on('error', common.mustNotCall());
    stream.on('slowRead', common.mustCall(() => {
      // Close without an error
      stream.close();
    }));
    stream.respond();
    stream.end(largeChunk);
  });

  server.listen(0, common.mustCall(() => {
    // Evil client creates a slow-read scenario, forcing our server to only
    // send one byte at a time.
    const client = http2.connect(`http://localhost:${server.address().port}`,
                                 { settings: { initialWindowSize: 1 } });

    const req = client.request();

    req.resume();
    req.on('close', () => {
      client.close();
      server.close();
    });

    req.on('close', common.mustCall());
    req.on('error', common.mustNotCall());
  }));
}
