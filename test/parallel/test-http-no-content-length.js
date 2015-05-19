'use strict';
var common = require('../common');
var assert = require('assert');
var net = require('net');
var http = require('http');

var body = '';

var server = net.createServer(function(socket) {
  // Neither Content-Length nor Connection
  socket.end('HTTP/1.1 200 ok\r\n\r\nHello');
}).listen(common.PORT, function() {
  var req = http.get({port: common.PORT}, function(res) {
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
