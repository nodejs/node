'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const http2 = require('http2');
const assert = require('node:assert');

const server = http2.createServer();

server.listen(0, common.mustCall(() => {
  assert.strictEqual(server[Symbol.asyncDispose].name, '[Symbol.asyncDispose]');
  server.on('close', common.mustCall());
  server[Symbol.asyncDispose]().then(common.mustCall(() => {
    assert.strictEqual(server._handle, null);

    // Disposer must be idempotent, subsequent call must not throw
    server[Symbol.asyncDispose]().then(common.mustCall());
  }));
}));
