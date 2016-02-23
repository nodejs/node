'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var serverGotEnd = false;
var clientGotEnd = false;

var server = net.createServer({allowHalfOpen: true}, function(socket) {
  socket.resume();
  socket.on('end', function() {
    serverGotEnd = true;
  });
  socket.end();
});

server.listen(common.PORT, function() {
  var client = net.connect({
    host: '127.0.0.1',
    port: common.PORT,
    allowHalfOpen: true
  }, function() {
    console.error('client connect cb');
    client.resume();
    client.on('end', function() {
      clientGotEnd = true;
      setTimeout(function() {
        assert(client.writable);
        client.end();
      }, 10);
    });
    client.on('close', function() {
      server.close();
    });
  });
});

process.on('exit', function() {
  console.error('exit', serverGotEnd, clientGotEnd);
  assert(serverGotEnd);
  assert(clientGotEnd);
});
