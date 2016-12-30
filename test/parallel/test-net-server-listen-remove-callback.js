'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

// Server should only fire listen callback once
var server = net.createServer();

server.on('close', function() {
  var listeners = server.listeners('listening');
  console.log('Closed, listeners:', listeners.length);
  assert.equal(0, listeners.length);
});

server.listen(0, function() {
  server.close();
});

server.once('close', function() {
  server.listen(0, function() {
    server.close();
  });
});
