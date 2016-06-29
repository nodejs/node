'use strict';
require('../common');
var net = require('net');
var http = require('http');

// This is a regression test for https://github.com/joyent/node/issues/44
// It is separate from test-http-malformed-request.js because it is only
// reproduceable on the first packet on the first connection to a server.

var server = http.createServer(function(req, res) {});
server.listen(0);

server.on('listening', function() {
  net.createConnection(this.address().port).on('connect', function() {
    this.destroy();
  }).on('close', function() {
    server.close();
  });
});
