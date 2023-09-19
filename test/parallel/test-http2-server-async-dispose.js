'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');

const server = http2.createServer();

server.listen(0, common.mustCall(() => {
  server.on('close', common.mustCall());
  server[Symbol.asyncDispose]().then(common.mustCall());
}));
