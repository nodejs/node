'use strict';

const net = require('net');
var errors = 0;

var server = net.Server(function(socket) {

  socket.on('error', function() {
    errors++;
  });

});

//server.maxConnections = 128;

server.listen(9000);

var oldConnections, oldErrors;

setInterval(function() {
  if (oldConnections != server.connections) {
    oldConnections = server.connections;
    console.log('SERVER %d connections: %d', process.pid, server.connections);
  }

  if (oldErrors != errors) {
    oldErrors = errors;
    console.log('SERVER %d errors: %d', process.pid, errors);
  }
}, 1000);
