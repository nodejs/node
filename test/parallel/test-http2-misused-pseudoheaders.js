'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const h2 = require('http2');

const server = h2.createServer();

server.on('stream', common.mustCall((stream) => {
  [
    ':path',
    ':authority',
    ':method',
    ':scheme'
  ].forEach((i) => {
    common.expectsError(() => stream.respond({ [i]: '/' }),
                        {
                          code: 'ERR_HTTP2_INVALID_PSEUDOHEADER'
                        });
  });

  stream.respond({}, {
    getTrailers: common.mustCall((trailers) => {
      trailers[':status'] = 'bar';
    })
  });

  stream.on('error', common.expectsError({
    code: 'ERR_HTTP2_INVALID_PSEUDOHEADER'
  }));

  stream.end('hello world');
}));


server.listen(0, common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('error', common.expectsError({
    code: 'ERR_HTTP2_STREAM_ERROR',
    type: Error,
    message: 'Stream closed with error code 2'
  }));

  req.on('response', common.mustCall());
  req.resume();
  req.on('end', common.mustCall());
  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
