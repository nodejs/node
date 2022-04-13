'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const server = net.createServer(common.mustNotCall());

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

server.listen(common.PIPE, common.mustCall(function() {
  assert.strictEqual(server.address(), common.PIPE);
  server.close();
}));
