'use strict';
const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');
const { kConnectionsCheckingInterval } = require('_http_server');

const server = createServer();

server.listen(0, common.mustCall(() => {
  assert.strictEqual(server[Symbol.asyncDispose].name, '[Symbol.asyncDispose]');
  server.on('close', common.mustCall());
  server[Symbol.asyncDispose]().then(common.mustCall(() => {
    assert(server[kConnectionsCheckingInterval]._destroyed);

    // Disposer must be idempotent, subsequent call must not throw
    server[Symbol.asyncDispose]().then(common.mustCall());
  }));
}));
