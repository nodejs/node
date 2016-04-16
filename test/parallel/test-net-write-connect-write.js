'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');

var received = '';

var server = net.createServer(function(socket) {
  socket.pipe(socket);
}).listen(common.PORT, function() {
  var conn = net.connect(common.PORT);
  conn.setEncoding('utf8');
  conn.write('before');
  conn.on('connect', function() {
    conn.write('after');
  });
  conn.on('data', function(buf) {
    received += buf;
    conn.end();
  });
  conn.on('end', function() {
    server.close();
  });
});

process.on('exit', function() {
  assert.equal(received, 'before' + 'after');
});
