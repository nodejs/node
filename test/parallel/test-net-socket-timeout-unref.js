'use strict';

// Test that unref'ed sockets with timeouts do not prevent exit.

const common = require('../common');
const net = require('net');

const server = net.createServer(function(c) {
  c.write('hello');
  c.unref();
});
server.listen(0);
server.unref();

var connections = 0;
const sockets = [];
const delays = [8, 5, 3, 6, 2, 4];

delays.forEach(function(T) {
  const socket = net.createConnection(server.address().port, 'localhost');
  socket.on('connect', common.mustCall(function() {
    if (++connections === delays.length) {
      sockets.forEach(function(s) {
        s.socket.setTimeout(s.timeout, function() {
          s.socket.destroy();
          throw new Error('socket timed out unexpectedly');
        });

        s.socket.unref();
      });
    }
  }));

  sockets.push({socket: socket, timeout: T * 1000});
});
