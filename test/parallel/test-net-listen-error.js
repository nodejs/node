'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var gotError = false;

var server = net.createServer(function(socket) {
});
server.listen(1, '1.1.1.1', function() { // EACCESS or EADDRNOTAVAIL
  assert(false);
});
server.on('error', function(error) {
  console.error(error);
  gotError = true;
});

process.on('exit', function() {
  assert(gotError);
});
