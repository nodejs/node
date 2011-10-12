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

server.listen(3000, function() {
  server.close();
  server.listen(3001, function() {
    server.close();
  });
});
