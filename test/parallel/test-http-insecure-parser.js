// Flags: --insecure-http-parser

'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const server = http.createServer(function(req, res) {
  assert.strictEqual(req.headers['content-type'], 'text/te\bt');
  req.pipe(res);
});

server.listen(0, common.mustCall(function() {
  const bufs = [];
  const client = net.connect(
    this.address().port,
    function() {
      client.write(
        'GET / HTTP/1.1\r\n' +
        'Content-Type: text/te\x08t\r\n' +
        'Connection: close\r\n\r\n');
    }
  );
  client.on('data', function(chunk) {
    bufs.push(chunk);
  });
  client.on('end', common.mustCall(function() {
    const head = Buffer.concat(bufs)
      .toString('latin1')
      .split('\r\n')[0];
    assert.strictEqual(head, 'HTTP/1.1 200 OK');
    server.close();
  }));
}));
