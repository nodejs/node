'use strict';

const common = require('../common');
const http = require('http');
const net = require('net');
const assert = require('assert');

const reqstr = common.tagCRLFy`
  HTTP/1.1 200 OK
  Content-Length: 1
  Transfer-Encoding: chunked


`;

const server = net.createServer((socket) => {
  socket.write(reqstr);
});

server.listen(0, () => {
  // The callback should not be called because the server is sending
  // both a Content-Length header and a Transfer-Encoding: chunked
  // header, which is a violation of the HTTP spec.
  const req = http.get({port: server.address().port}, common.mustNotCall());
  req.on('error', common.mustCall((err) => {
    assert(/^Parse Error/.test(err.message));
    assert.strictEqual(err.code, 'HPE_UNEXPECTED_CONTENT_LENGTH');
    server.close();
  }));
});
