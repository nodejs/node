'use strict';
require('../common');
var assert = require('assert');
var net = require('net');

var server = net.createServer(function(socket) {
});
server.listen(0, function() {
  assert(false);
});
server.on('error', function(error) {
  console.error(error);
  assert(false);
});
server.close();
