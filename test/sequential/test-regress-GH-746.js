'use strict';
// Just test that destroying stdin doesn't mess up listening on a server.
// This is a regression test for GH-746.

const common = require('../common');
const assert = require('assert');
const net = require('net');

process.stdin.destroy();

var accepted = null;
var server = net.createServer(function(socket) {
  console.log('accepted');
  accepted = socket;
  socket.end();
  server.close();
});


server.listen(common.PORT, function() {
  console.log('listening...');

  net.createConnection(common.PORT);
});


process.on('exit', function() {
  assert.ok(accepted);
});

