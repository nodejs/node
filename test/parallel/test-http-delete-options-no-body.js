'use strict';
const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');
const net = require('node:net');

// Test backward compatibility: DELETE and OPTIONS requests without a body
// should NOT add Content-Length or Transfer-Encoding headers.
// Ref: https://github.com/nodejs/node/issues/27880

let testCount = 0;
const totalTests = 2;

// Test 1: DELETE with no body should NOT add Content-Length/Transfer-Encoding
const server1 = net.createServer(common.mustCall((socket) => {
  let data = '';
  socket.on('data', (chunk) => {
    data += chunk.toString();
    if (data.includes('\r\n\r\n')) {
      const headerEnd = data.indexOf('\r\n\r\n');
      const headers = data.substring(0, headerEnd);

      // Should NOT have Content-Length or Transfer-Encoding
      const hasContentLength = /Content-Length:/i.test(headers);
      const hasTransferEncoding = /Transfer-Encoding:/i.test(headers);

      assert(!hasContentLength && !hasTransferEncoding,
             'DELETE without body should not have Content-Length or Transfer-Encoding');

      testCount++;
      socket.write('HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK');
      socket.end();
      server1.close();
      runTest2();
    }
  });
}));

server1.listen(0, common.mustCall(() => {
  const req = http.request({ method: 'DELETE', port: server1.address().port });
  req.end();  // No body
}));

// Test 2: OPTIONS with no body
function runTest2() {
  const server2 = net.createServer(common.mustCall((socket) => {
    let data = '';
    socket.on('data', (chunk) => {
      data += chunk.toString();
      if (data.includes('\r\n\r\n')) {
        const headerEnd = data.indexOf('\r\n\r\n');
        const headers = data.substring(0, headerEnd);

        const hasContentLength = /Content-Length:/i.test(headers);
        const hasTransferEncoding = /Transfer-Encoding:/i.test(headers);

        assert(!hasContentLength && !hasTransferEncoding,
               'OPTIONS without body should not have Content-Length or Transfer-Encoding');

        testCount++;
        socket.write('HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK');
        socket.end();
        server2.close();

        process.on('exit', () => {
          assert.strictEqual(testCount, totalTests);
        });
      }
    });
  }));

  server2.listen(0, common.mustCall(() => {
    const req = http.request({ method: 'OPTIONS', port: server2.address().port });
    req.end();  // No body
  }));
}
