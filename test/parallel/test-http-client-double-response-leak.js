'use strict';

// Test for memory leak when server sends double HTTP response
// Refs: https://github.com/nodejs/node/issues/60025

const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// This test creates a scenario where the server sends two complete HTTP
// responses in a single TCP chunk, which puts the HTTP parser in an invalid
// state. Without the fix, the parser is never freed because the cleanup
// logic checks parser.incoming.complete, but parser.incoming points to the
// incomplete second response that will never finish.

async function testDoubleResponseLeak() {
  const iterations = 1000;
  const memBefore = process.memoryUsage().heapUsed;

  for (let i = 0; i < iterations; i++) {
    await new Promise((resolve) => {
      // Create a raw TCP server that will send double HTTP response
      const server = net.createServer((socket) => {
        // Send two complete HTTP responses in one chunk
        socket.write(
          'HTTP/1.1 200 OK\r\n' +
          'Content-Length: 5\r\n' +
          '\r\n' +
          'first' +
          'HTTP/1.1 200 OK\r\n' +
          'Content-Length: 6\r\n' +
          '\r\n' +
          'second'
        );
        socket.end();
      });

      server.listen(0, common.mustCall(() => {
        const req = http.get(`http://127.0.0.1:${server.address().port}`);

        req.on('response', common.mustCall((res) => {
          res.resume();
          res.on('end', () => {
            // Response ended normally
          });
        }));

        req.on('error', () => {
          // Expected error due to socket destruction on double response
        });

        req.on('close', () => {
          server.close(() => {
            resolve();
          });
        });
      }));
    });

    // Force GC every 100 iterations to verify parsers are being freed
    if (i % 100 === 0 && global.gc) {
      global.gc();
      await new Promise(setImmediate);
    }
  }

  if (global.gc) {
    global.gc();
    await new Promise(setImmediate);
  }

  const memAfter = process.memoryUsage().heapUsed;
  const growth = memAfter - memBefore;
  const growthMB = growth / 1024 / 1024;

  console.log(`Memory growth: ${growthMB.toFixed(2)} MB`);

  // With the fix, memory growth should be minimal (< 10 MB for 1000 iterations)
  // Without the fix, each iteration leaks a parser (~500 bytes + buffers),
  // leading to growth of 50+ MB
  assert.ok(growthMB < 10,
    `Excessive memory growth: ${growthMB.toFixed(2)} MB (expected < 10 MB)`);
}

(async () => {
  console.log('Testing HTTP client double response memory leak...');
  await testDoubleResponseLeak();
  console.log('Test passed!');
})().catch(common.mustNotCall());
