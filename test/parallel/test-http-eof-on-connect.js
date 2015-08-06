'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');
const http = require('http');

// This is a regression test for https://github.com/joyent/node/issues/44
// It is separate from test-http-malformed-request.js because it is only
// reproduceable on the first packet on the first connection to a server.

var server = http.createServer(function(req, res) {});
server.listen(common.PORT);

server.on('listening', function() {
  net.createConnection(common.PORT).on('connect', function() {
    this.destroy();
  }).on('close', function() {
    server.close();
  });
});
