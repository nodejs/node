'use strict';
require('../common');
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

server.listen(0, function() {
  var client = net.connect(this.address().port, function() {
    client.end();
  });
});
