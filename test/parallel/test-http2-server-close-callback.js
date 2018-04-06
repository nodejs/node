'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');

const server = http2.createServer();

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  client.on('error', (err) => {
    if (err.code !== 'ECONNRESET')
      throw err;
  });
}));

server.on('session', common.mustCall((s) => {
  setImmediate(() => {
    server.close(common.mustCall());
    s.destroy();
  });
}));
