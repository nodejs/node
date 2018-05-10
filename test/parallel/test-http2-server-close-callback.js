'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');

const server = http2.createServer();

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  // depending on network events, this might or might not be emitted
  client.on('error', () => {});

  client.on('connect', common.mustCall(() => {
    server.close(common.mustCall());
  }));
}));

server.on('session', common.mustCall((s) => {
  setImmediate(() => {
    s.destroy();
  });
}));
