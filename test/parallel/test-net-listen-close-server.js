'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
var gotError = false;

var server = net.createServer(function(socket) {
});
server.listen(common.PORT, function() {
  assert(false);
});
server.on('error', function(error) {
  common.debug(error);
  assert(false);
});
server.close();
