'use strict';
const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');
const net = require('node:net');

const testData = 'OPTIONS request body data';

// Test that OPTIONS requests with a body get proper Content-Length or
// Transfer-Encoding headers to prevent malformed HTTP and request smuggling.
// Ref: https://github.com/nodejs/node/issues/27880

const server = net.createServer(common.mustCall((socket) => {
  let receivedData = '';

  socket.on('data', (chunk) => {
    receivedData += chunk.toString();

    if (receivedData.includes('\r\n\r\n')) {
      const headerEnd = receivedData.indexOf('\r\n\r\n');
      const headers = receivedData.substring(0, headerEnd);

      const hasContentLength = /Content-Length:\s*\d+/i.test(headers);
      const hasTransferEncoding = /Transfer-Encoding:\s*chunked/i.test(headers);

      assert(hasContentLength || hasTransferEncoding,
             'OPTIONS request with body must have Content-Length or Transfer-Encoding header');

      socket.write('HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK');
      socket.end();
    }
  });
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  const req = http.request({
    method: 'OPTIONS',
    port: port,
    host: 'localhost',
    path: '/'
  }, common.mustCall((res) => {
    res.resume();
    res.on('end', () => {
      server.close();
    });
  }));

  req.write(testData);
  req.end();
}));
