'use strict';

const common = require('../common');
const assert = require('assert');
const { createServer } = require('http');

const server = createServer({
  connectionsCheckingInterval: 1,
}, common.mustNotCall());

// Invalid headersTimeout should not crash the server
server.headersTimeout = 'im-not-a-number';
assert.strictEqual(server.headersTimeout, 'im-not-a-number');

server.listen(0, '127.0.0.1', common.mustCall(() => {
  setTimeout(common.mustCall(() => {
    server.close(common.mustCall());
  }), common.platformTimeout(50));
}));
