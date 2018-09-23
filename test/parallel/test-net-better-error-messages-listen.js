'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var server = net.createServer(common.fail);
server.listen(1, '1.1.1.1', common.fail);
server.on('error', common.mustCall(function(e) {
  assert.equal(e.address, '1.1.1.1');
  assert.equal(e.port, 1);
  assert.equal(e.syscall, 'listen');
}));
