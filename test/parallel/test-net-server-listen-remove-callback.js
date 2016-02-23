'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

// Server should only fire listen callback once
var server = net.createServer();

server.on('close', function() {
  var listeners = server.listeners('listening');
  console.log('Closed, listeners:', listeners.length);
  assert.equal(0, listeners.length);
});

server.listen(common.PORT, function() {
  server.close();
});

server.once('close', function() {
  server.listen(common.PORT + 1, function() {
    server.close();
  });
});
