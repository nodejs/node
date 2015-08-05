'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
var gotError = false;

var server = net.createServer(function(socket) {
});
server.listen(1, '1.1.1.1', function() { // EACCESS or EADDRNOTAVAIL
  assert(false);
});
server.on('error', function(error) {
  common.debug(error);
  gotError = true;
});

process.on('exit', function() {
  assert(gotError);
});
