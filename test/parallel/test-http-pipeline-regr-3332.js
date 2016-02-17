'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const big = new Buffer(16 * 1024);
big.fill('A');

const COUNT = 1e4;

var received = 0;

var client;
const server = http.createServer(function(req, res) {
  res.end(big, function() {
    if (++received === COUNT) {
      server.close();
      client.end();
    }
  });
}).listen(common.PORT, function() {
  var req = new Array(COUNT + 1).join('GET / HTTP/1.1\r\n\r\n');
  client = net.connect(common.PORT, function() {
    client.write(req);
  });

  // Just let the test terminate instead of hanging
  client.on('close', function() {
    if (received !== COUNT)
      server.close();
  });
  client.resume();
});

process.on('exit', function() {
  // The server should pause connection on pipeline flood, but it shoul still
  // resume it and finish processing the requests, when its output queue will
  // be empty again.
  assert.equal(received, COUNT);
});
