'use strict';
const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');
const net = require('node:net');

// Test that DELETE requests with bodies cannot be used for request smuggling
// attacks by verifying proper headers are sent.
// Ref: https://github.com/nodejs/node/issues/27880

const maliciousPayload = 'POST /admin HTTP/1.1\r\nHost: victim.com\r\n\r\n';

const server = net.createServer(common.mustCall((socket) => {
  let data = '';

  socket.on('data', (chunk) => {
    data += chunk.toString();

    // Wait for complete request (handle both chunked and content-length)
    const hasCompleteChunked = data.includes('0\r\n\r\n');
    const hasCompleteContentLength = data.includes('\r\n\r\n') &&
                                      /Content-Length:\s*(\d+)/i.test(data);

    if (hasCompleteChunked || hasCompleteContentLength) {
      const headerEnd = data.indexOf('\r\n\r\n');
      const headers = data.substring(0, headerEnd);
      const body = data.substring(headerEnd + 4);

      // Verify headers are present
      const hasContentLength = /Content-Length:\s*(\d+)/i.exec(headers);
      const hasTransferEncoding = /Transfer-Encoding:\s*chunked/i.test(headers);

      assert(hasContentLength || hasTransferEncoding,
             'Request must have length headers to prevent smuggling');

      // If chunked encoding, verify payload is properly encoded
      if (hasTransferEncoding) {
        // Chunked body should start with hex size, not raw HTTP
        assert(!body.startsWith('POST '),
               'Body should be chunked-encoded, not raw HTTP');
        // Should contain the chunk size in hex
        assert(/^[0-9a-fA-F]+\r\n/.test(body),
               'Chunked body should start with hex chunk size');
      }

      // If Content-Length, verify it matches actual body length
      if (hasContentLength) {
        const declaredLength = parseInt(hasContentLength[1], 10);
        assert.strictEqual(declaredLength, maliciousPayload.length);
      }

      socket.write('HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK');
      socket.end();
    }
  });
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;

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

  // Attempt to inject malicious HTTP request as body
  req.write(maliciousPayload);
  req.end();
}));
