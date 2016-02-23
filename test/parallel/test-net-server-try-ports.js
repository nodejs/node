'use strict';
// This tests binds to one port, then attempts to start a server on that
// port. It should be EADDRINUSE but be able to then bind to another port.
var common = require('../common');
var assert = require('assert');
var net = require('net');

var connections = 0;

var server1listening = false;
var server2listening = false;
var server2eaddrinuse = false;

var server1 = net.Server(function(socket) {
  connections++;
  socket.destroy();
});

var server2 = net.Server(function(socket) {
  connections++;
  socket.destroy();
});

var server2errors = 0;
server2.on('error', function(e) {
  server2errors++;
  console.error('server2 error');

  if (e.code == 'EADDRINUSE') {
    server2eaddrinuse = true;
  }

  server2.listen(common.PORT + 1, function() {
    console.error('server2 listening');
    server2listening = true;

    server1.close();
    server2.close();
  });
});


server1.listen(common.PORT, function() {
  console.error('server1 listening');
  server1listening = true;
  // This should make server2 emit EADDRINUSE
  server2.listen(common.PORT);
});


process.on('exit', function() {
  assert.equal(1, server2errors);
  assert.ok(server2eaddrinuse);
  assert.ok(server2listening);
  assert.ok(server1listening);
});
