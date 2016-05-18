'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer();

assert.strictEqual(server.listening, false);

server.listen(common.PORT, common.mustCall(() => {
  assert.strictEqual(server.listening, true);

  server.close(common.mustCall(() => {
    assert.strictEqual(server.listening, false);
  }));
}));
