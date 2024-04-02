'use strict';

const common = require('../common');
const net = require('net');
const assert = require('assert');

const server = net.createServer();

server.on('connection', common.mustCall());

// Ensure that the socket is not destroyed when the 'end' event is emitted.

server.listen(common.mustCall(function() {
  const socket = net.createConnection({
    port: server.address().port
  });

  socket.on('connect', common.mustCall(function() {
    socket.on('end', common.mustCall(function() {
      assert.strictEqual(socket.destroyed, false);
      server.close();
    }));

    socket.end();
  }));
}));
