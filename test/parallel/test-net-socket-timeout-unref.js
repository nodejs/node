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
var connections = 0;
var sockets = [];
var delays = [8, 5, 3, 6, 2, 4];

delays.forEach(function(T) {
  var socket = net.createConnection(common.PORT, 'localhost');
  socket.on('connect', function() {
    if (++connections === delays.length) {
      sockets.forEach(function(s) {
        s[0].setTimeout(s[1] * 1000, function() {
          timedout = true;
          s[0].destroy();
        });

        s[0].unref();
      });
    }
  });

  sockets.push([socket, T]);
});

process.on('exit', function() {
  assert.strictEqual(timedout, false,
                     'Socket timeout should not hold loop open');
});
