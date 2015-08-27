'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var address = null;

var server = net.createServer(function() {
  assert(false); // should not be called
});

server.listen(common.PIPE, function() {
  address = server.address();
  server.close();
});

process.on('exit', function() {
  assert.equal(address, common.PIPE);
});
