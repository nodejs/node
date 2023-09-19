'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { createServer } = require('https');
const { kConnectionsCheckingInterval } = require('_http_server');

const server = createServer();

server.listen(0, common.mustCall(() => {
  server.on('close', common.mustCall());
  server[Symbol.asyncDispose]().then(common.mustCall(() => {
    assert(server[kConnectionsCheckingInterval]._destroyed);
  }));
}));
