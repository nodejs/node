'use strict';
const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');
const net = require('node:net');

const testData = 'DELETE request body data';

// Test that DELETE requests with a body get proper Content-Length or
// Transfer-Encoding headers to prevent malformed HTTP and request smuggling.
// Ref: https://github.com/nodejs/node/issues/27880

const server = net.createServer(common.mustCall((socket) => {
  let receivedData = '';

  socket.on('data', (chunk) => {
    receivedData += chunk.toString();

    // Check for complete request (headers + body)
    if (receivedData.includes('\r\n\r\n')) {
      const headerEnd = receivedData.indexOf('\r\n\r\n');
      const headers = receivedData.substring(0, headerEnd);

      // Verify either Content-Length or Transfer-Encoding is present
      const hasContentLength = /Content-Length:\s*\d+/i.test(headers);
      const hasTransferEncoding = /Transfer-Encoding:\s*chunked/i.test(headers);

      assert(hasContentLength || hasTransferEncoding,
             'DELETE request with body must have Content-Length or Transfer-Encoding header');

      // Send response
      socket.write('HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK');
      socket.end();
    }
  });
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

  // Test DELETE with body
  const req = http.request({
    method: 'DELETE',
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
