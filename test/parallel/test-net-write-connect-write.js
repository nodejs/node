'use strict';
require('../common');
var assert = require('assert');
var net = require('net');

var received = '';

var server = net.createServer(function(socket) {
  socket.pipe(socket);
}).listen(0, function() {
  var conn = net.connect(this.address().port);
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
