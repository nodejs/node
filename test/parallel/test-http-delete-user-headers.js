'use strict';
const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');
const net = require('node:net');

// Test that user-specified Content-Length and Transfer-Encoding headers
// are respected and not overridden by the automatic header logic.
// Ref: https://github.com/nodejs/node/issues/27880

let testCount = 0;
const totalTests = 2;

// Test 1: User sets Content-Length manually
const server1 = net.createServer(common.mustCall((socket) => {
  let data = '';
  socket.on('data', (chunk) => {
    data += chunk.toString();
    if (data.includes('\r\n\r\n')) {
      const headers = data.substring(0, data.indexOf('\r\n\r\n'));

      // Should use user's Content-Length
      assert(/Content-Length:\s*9/i.test(headers),
             'Should respect user-specified Content-Length');

      testCount++;
      socket.write('HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK');
      socket.end();
      server1.close();
      runTest2();
    }
  });
}));

server1.listen(0, common.mustCall(() => {
  const req = http.request({
    method: 'DELETE',
    port: server1.address().port,
    headers: { 'Content-Length': '9' }
  });
  req.end('test data');
}));

// Test 2: User sets Transfer-Encoding manually
function runTest2() {
  const server2 = net.createServer(common.mustCall((socket) => {
    let data = '';
    socket.on('data', (chunk) => {
      data += chunk.toString();
      if (data.includes('0\r\n\r\n')) {  // End of chunked encoding
        const headers = data.substring(0, data.indexOf('\r\n\r\n'));

        // Should use user's Transfer-Encoding
        assert(/Transfer-Encoding:\s*chunked/i.test(headers),
               'Should respect user-specified Transfer-Encoding');

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
    const req = http.request({
      method: 'DELETE',
      port: server2.address().port,
      headers: { 'Transfer-Encoding': 'chunked' }
    });
    req.write('test data');
    req.end();
  }));
}
