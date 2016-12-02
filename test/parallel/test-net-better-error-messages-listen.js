'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const server = net.createServer(common.fail);
server.listen(1, '1.1.1.1', common.fail);
server.on('error', common.mustCall(function(e) {
  assert.strictEqual(e.address, '1.1.1.1');
  assert.strictEqual(e.port, 1);
  assert.strictEqual(e.syscall, 'listen');
}));
