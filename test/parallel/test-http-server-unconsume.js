'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

let received = '';

const server = http.createServer(function(req, res) {
  res.writeHead(200);
  res.end();

  req.socket.on('data', function(data) {
    received += data;
  });

  server.close();
}).listen(0, function() {
  const socket = net.connect(this.address().port, function() {
    socket.write('PUT / HTTP/1.1\r\n\r\n');

    socket.once('data', function() {
      socket.end('hello world');
    });
  });
});

process.on('exit', function() {
  assert.strictEqual(received, 'hello world');
});
