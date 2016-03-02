'use strict';
const net = require('net');

var errors = 0, connections = 0;

function connect() {
  process.nextTick(function() {
    var s = net.Stream();
    var gotConnected = false;
    s.connect(9000);

    s.on('connect', function() {
      gotConnected = true;
      connections++;
      connect();
    });

    s.on('close', function() {
      if (gotConnected) connections--;
    });

    s.on('error', function() {
      errors++;
    });
  });
}

connect();


var oldConnections, oldErrors;

// Try to start new connections every so often
setInterval(connect, 5000);

setInterval(function() {
  if (oldConnections != connections) {
    oldConnections = connections;
    console.log('CLIENT %d connections: %d', process.pid, connections);
  }

  if (oldErrors != errors) {
    oldErrors = errors;
    console.log('CLIENT %d errors: %d', process.pid, errors);
  }
}, 1000);

