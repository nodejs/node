'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const server = http.createServer(common.mustCall((req, res) => {
  const limit = server.maxHeadersCount * 2;
  assert.ok(req.rawHeaders.length <= limit,
            `rawHeaders.length (${req.rawHeaders.length}) exceeds limit (${limit})`);
  res.end();
  server.close();
}));

server.maxHeadersCount = 50;

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const headers = Array.from({ length: 65 }, (_, i) => `X-${i}:v`).join('\r\n');
  const req = `GET / HTTP/1.1\r\nHost: localhost\r\n${headers}\r\n\r\n`;

  net.createConnection(port, 'localhost', function() {
    this.write(req);
    this.once('data', () => this.end());
  });
}));
