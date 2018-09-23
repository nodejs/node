'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var server = net.createServer(function(c) {
  c.write('hello');
  c.unref();
});
server.listen(common.PORT);
server.unref();

var timedout = false;

[8, 5, 3, 6, 2, 4].forEach(function(T) {
  var socket = net.createConnection(common.PORT, 'localhost');
  socket.setTimeout(T * 1000, function() {
    console.log(process._getActiveHandles());
    timedout = true;
    socket.destroy();
  });
  socket.unref();
});

process.on('exit', function() {
  assert.strictEqual(timedout, false,
                     'Socket timeout should not hold loop open');
});
