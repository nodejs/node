'use strict';
require('../common');
var assert = require('assert');
var net = require('net');
var http = require('http');

var body = '';

var server = net.createServer(function(socket) {
  // Neither Content-Length nor Connection
  socket.end('HTTP/1.1 200 ok\r\n\r\nHello');
}).listen(0, function() {
  http.get({port: this.address().port}, function(res) {
    res.setEncoding('utf8');
    res.on('data', function(chunk) {
      body += chunk;
    });
    res.on('end', function() {
      server.close();
    });
  });
});

process.on('exit', function() {
  assert.equal(body, 'Hello');
});
