'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

var address = null;

const server = net.createServer(function() {
  assert(false); // should not be called
});

server.listen(common.PIPE, function() {
  address = server.address();
  server.close();
});

process.on('exit', function() {
  assert.equal(address, common.PIPE);
});
