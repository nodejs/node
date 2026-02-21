'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');

const server = http2.createServer();

server.listen(0, () => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  client.on('close', common.mustCall(() => {
    server.close();
  }));

  // The client.close() is executed before the socket is able to make request
  const stream = client.request();
  stream.on('error', common.expectsError({ code: 'ERR_HTTP2_GOAWAY_SESSION' }));

  setImmediate(() => client.close());
});
