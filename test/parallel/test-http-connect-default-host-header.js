'use strict';

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

const target = 'target.example.com:443';

const server = net.createServer(common.mustCall((socket) => {
  socket.once('data', common.mustCall((data) => {
    const rawRequest = data.toString();
    const requestLines = rawRequest.split('\r\n');

    assert.strictEqual(requestLines[0], `CONNECT ${target} HTTP/1.1`);
    assert(requestLines.includes(`Host: ${target}`));

    socket.end('HTTP/1.1 200 Connection established\r\n\r\n');
  }));
}));

server.listen(0, common.localhostIPv4, common.mustCall(() => {
  const req = http.request({
    host: common.localhostIPv4,
    port: server.address().port,
    method: 'CONNECT',
    path: target,
  }, common.mustNotCall());

  req.on('connect', common.mustCall((res, socket) => {
    assert.strictEqual(res.statusCode, 200);
    socket.destroy();
    server.close();
  }));

  req.end();
}));
