'use strict';
require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const COUNT = 10;

let received = 0;

const server = http.createServer(function(req, res) {
  // Close the server, we have only one TCP connection anyway
  if (received++ === 0)
    server.close();

  res.writeHead(200);
  res.write('data');

  setTimeout(function() {
    res.end();
  }, (Math.random() * 100) | 0);
}).listen(0, function() {
  const s = net.connect(this.address().port);

  const big = 'GET / HTTP/1.0\r\n\r\n'.repeat(COUNT);

  s.write(big);
  s.resume();
});

process.on('exit', function() {
  assert.strictEqual(received, COUNT);
});
