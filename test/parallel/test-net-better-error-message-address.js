var common = require('../common');
var assert = require('assert');
var net = require('net');

var server = net.createServer(assert.fail);
server.listen(1, assert.fail);
server.on('error', common.mustCall(function(e) {
  assert.equal(e.address, '0.0.0.0');
  assert.equal(e.port, 1);
  assert.equal(e.syscall, 'listen');
  assert.equal(e.message, 'listen EACCES 0.0.0.0:1');
}));
