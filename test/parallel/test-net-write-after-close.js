'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var gotError = false;
var gotWriteCB = false;

process.on('exit', function() {
  assert(gotError);
  assert(gotWriteCB);
});

var server = net.createServer(function(socket) {
  socket.resume();

  socket.on('error', function(error) {
    console.error('got error, closing server', error);
    server.close();
    gotError = true;
  });

  setTimeout(function() {
    console.error('about to try to write');
    socket.write('test', function(e) {
      gotWriteCB = true;
    });
  }, 250);
});

server.listen(common.PORT, function() {
  var client = net.connect(common.PORT, function() {
    client.end();
  });
});
