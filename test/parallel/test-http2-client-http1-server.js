'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');
const http = require('http');

const server = http.createServer();
server.on('stream', common.mustNotCall());

server.listen(0, common.mustCall(() => {
  server.unref();

  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('streamClosed', common.mustCall());

  client.on('error', common.expectsError({
    code: 'ERR_HTTP2_PROTOCOL_ERROR',
    type: Error,
    message: 'An HTTP2 protocol error occurred: Remote peer returned ' +
              'unexpected data while we expected SETTINGS frame.  Perhaps, ' +
              'peer does not support HTTP/2 properly.'
  }));
}));
