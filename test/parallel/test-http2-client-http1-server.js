'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http = require('http');
const http2 = require('http2');

const server = http.createServer(common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const req = client.request();
  req.on('close', common.mustCall());

  client.on('error', common.expectsError({
    code: 'ERR_HTTP2_ERROR',
    type: Error,
    message: 'Protocol error'
  }));

  client.on('close', (...args) => {
    server.close();
  });
}));
