'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var gotError = false;

var server = net.createServer(function(socket) {
});
server.listen(common.PORT, function() {
  assert(false);
});
server.on('error', function(error) {
  console.error(error);
  assert(false);
});
server.close();
