// Flags: --expose-internals --no-warnings

'use strict';
const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { kConnectionsCheckingInterval } = require('internal/http/server');

const server = createServer();

server.listen(0, common.mustCall(() => {
  server.on('close', common.mustCall());
  server[Symbol.asyncDispose]().then(common.mustCall(() => {
    assert(server[kConnectionsCheckingInterval]._destroyed);
  }));
}));
