'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  const session = stream.session;
  session.goaway(1);
  session.goaway(2);
  stream.session.on('close', common.mustCall(() => {
    common.expectsError(
      () => session.goaway(3),
      {
        code: 'ERR_HTTP2_INVALID_SESSION',
        type: Error
      }
    );
  }));
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  client.on('error', common.expectsError({
    code: 'ERR_HTTP2_SESSION_ERROR'
  }));

  const req = client.request();
  req.on('error', common.expectsError({
    code: 'ERR_HTTP2_SESSION_ERROR'
  }));
  req.resume();
  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));
}));
