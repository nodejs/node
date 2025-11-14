'use strict';
const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');
const net = require('node:net');
const { Readable } = require('stream');

// Test various methods of writing body data with DELETE and OPTIONS requests.
// All methods should result in proper headers being added.
// Ref: https://github.com/nodejs/node/issues/27880

let testCount = 0;
const totalTests = 6;

function checkHeaders(headers, testName) {
  const hasContentLength = /Content-Length:\s*\d+/i.test(headers);
  const hasTransferEncoding = /Transfer-Encoding:\s*chunked/i.test(headers);

  assert(hasContentLength || hasTransferEncoding,
         `${testName}: Must have Content-Length or Transfer-Encoding header`);
}

function createServer(callback) {
  const server = net.createServer((socket) => {
    let data = '';
    socket.on('data', (chunk) => {
      data += chunk.toString();
      // For chunked encoding, wait for the terminating chunk
      if (data.includes('\r\n\r\n') &&
          (data.includes('0\r\n\r\n') || /Content-Length:\s*\d+/i.test(data))) {
        const headerEnd = data.indexOf('\r\n\r\n');
        const headers = data.substring(0, headerEnd);
        callback(headers);
        socket.write('HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nOK');
        socket.end();
      }
    });
  });
  return server;
}

// Test 1: DELETE with write() then end()
const server1 = createServer((headers) => {
  checkHeaders(headers, 'DELETE with write()');
  testCount++;
  server1.close();
  runTest2();
});

server1.listen(0, common.mustCall(() => {
  const req = http.request({ method: 'DELETE', port: server1.address().port });
  req.write('test data');
  req.end();
}));

// Test 2: DELETE with end(data)
function runTest2() {
  const server2 = createServer((headers) => {
    checkHeaders(headers, 'DELETE with end(data)');
    // Should have Content-Length for known length
    assert(/Content-Length:\s*9/i.test(headers),
           'DELETE with end(data) should have Content-Length');
    testCount++;
    server2.close();
    runTest3();
  });

  server2.listen(0, common.mustCall(() => {
    const req = http.request({ method: 'DELETE', port: server2.address().port });
    req.end('test data');
  }));
}

// Test 3: OPTIONS with write() then end()
function runTest3() {
  const server3 = createServer((headers) => {
    checkHeaders(headers, 'OPTIONS with write()');
    testCount++;
    server3.close();
    runTest4();
  });

  server3.listen(0, common.mustCall(() => {
    const req = http.request({ method: 'OPTIONS', port: server3.address().port });
    req.write('test data');
    req.end();
  }));
}

// Test 4: OPTIONS with end(data)
function runTest4() {
  const server4 = createServer((headers) => {
    checkHeaders(headers, 'OPTIONS with end(data)');
    assert(/Content-Length:\s*9/i.test(headers),
           'OPTIONS with end(data) should have Content-Length');
    testCount++;
    server4.close();
    runTest5();
  });

  server4.listen(0, common.mustCall(() => {
    const req = http.request({ method: 'OPTIONS', port: server4.address().port });
    req.end('test data');
  }));
}

// Test 5: DELETE with multiple write() calls
function runTest5() {
  const server5 = createServer((headers) => {
    checkHeaders(headers, 'DELETE with multiple write()');
    testCount++;
    server5.close();
    runTest6();
  });

  server5.listen(0, common.mustCall(() => {
    const req = http.request({ method: 'DELETE', port: server5.address().port });
    req.write('chunk1');
    req.write('chunk2');
    req.end();
  }));
}

// Test 6: DELETE with stream pipe
function runTest6() {
  const server6 = createServer((headers) => {
    checkHeaders(headers, 'DELETE with stream pipe');
    testCount++;
    server6.close();

    process.on('exit', () => {
      assert.strictEqual(testCount, totalTests);
    });
  });

  server6.listen(0, common.mustCall(() => {
    const req = http.request({ method: 'DELETE', port: server6.address().port });
    const stream = Readable.from(['test', ' ', 'data']);
    stream.pipe(req);
  }));
}
