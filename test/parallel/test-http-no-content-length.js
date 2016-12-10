'use strict';
const common = require('../common');
var assert = require('assert');
var net = require('net');
var http = require('http');

var server = net.createServer(function(socket) {
  // Neither Content-Length nor Connection
  socket.end('HTTP/1.1 200 ok\r\n\r\nHello');
}).listen(0, common.mustCall(function() {
  http.get({port: this.address().port}, common.mustCall(function(res) {
    var body = '';

    res.setEncoding('utf8');
    res.on('data', function(chunk) {
      body += chunk;
    });
    res.on('end', common.mustCall(function() {
      assert.strictEqual(body, 'Hello');
      server.close();
    }));
  }));
}));
