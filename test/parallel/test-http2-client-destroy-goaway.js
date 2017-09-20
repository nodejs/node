// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');

const server = http2.createServer();
server.on('stream', common.mustCall((stream) => {
  stream.on('error', common.mustCall());
  stream.session.shutdown();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  client.on('goaway', common.mustCall(() => {
    // We ought to be able to destroy the client in here without an error
    server.close();
    client.destroy();
  }));

  client.request();
}));
