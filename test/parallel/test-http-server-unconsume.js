'use strict';
require('../common');
var assert = require('assert');
var http = require('http');
var net = require('net');

var received = '';

var server = http.createServer(function(req, res) {
  res.writeHead(200);
  res.end();

  req.socket.on('data', function(data) {
    received += data;
  });

  server.close();
}).listen(0, function() {
  var socket = net.connect(this.address().port, function() {
    socket.write('PUT / HTTP/1.1\r\n\r\n');

    socket.once('data', function() {
      socket.end('hello world');
    });
  });
});

process.on('exit', function() {
  assert.equal(received, 'hello world');
});
