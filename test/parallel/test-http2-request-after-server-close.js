'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); };
const fixtures = require('../common/fixtures');
// This test ensure that the server will not accept any new request
// after server close is called.
const assert = require('assert');
const http2 = require('http2');

const { test } = require('node:test');

/**
 * Create and manage an HTTP/2 client stream with controlled write patterns
 * @param {http2.ClientHttp2Session} client - The HTTP/2 client session
 * @param {string} clientId - Identifier for the client (e.g., '1', '2')
 * @param {number} writeCount - Number of writes to perform
 * @param {number} writeInterval - Interval between writes in ms
 * @returns {object} - Object containing stream, status tracking, and functions
 */
function createClientStream(client, clientId, writeCount, writeInterval = 100) {
  let currentWriteCount = 0;
  let intervalId = null;
  let streamClosed = false;

  // Create the request
  const req = client.request({
    ':path': `/client${clientId}`,
    ':method': 'POST',
    'client-id': clientId,
    'content-type': 'text/plain'
  });

  // Set up event handlers
  req.on('response', (_) => {});

  req.on('data', (_) => {});

  req.on('end', () => {
    streamClosed = true;
  });

  req.on('close', () => {
    streamClosed = true;
    if (intervalId) {
      clearInterval(intervalId);
      intervalId = null;
    }
  });

  req.on('error', (err) => {
    if (intervalId) {
      clearInterval(intervalId);
      intervalId = null;
    }
  });

  // Start the write interval
  intervalId = setInterval(() => {
    currentWriteCount++;
    if (currentWriteCount > writeCount) {
      if (intervalId) {
        clearInterval(intervalId);
        intervalId = null;
      }
      req.close();
      return;
    }

    req.write(`Client ${clientId} write #${currentWriteCount}\n`);
  }, writeInterval);

  // Return object with stream, status tracking, and cleanup function
  return {
    stream: req,
    getWriteCount: () => currentWriteCount,
    isActive: () => !streamClosed && !req.destroyed && !req.closed,
  };
}

// This test start a server and create a client. Client open a request and
// send 20 writes at interval of 100ms and then close at 2000ms from server start.
// Server close is fired after 1000ms from server start.
// Same client open another request after 1500ms from server start and tries to
// send 10 writes at interval of 100ms but failed to connect as server close is already fired at 1000ms.
// Request 1 from client is gracefully closed after accepting all 20 writes as it started before server close fired.
// server successfully closes gracefully after receiving all 20 writes from client and also server refused to accept any new request.
test('HTTP/2 server close with existing and new requests', async () => {

  // Server setup
  const server = http2.createSecureServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem')
  });

  // Track server events
  let serverStart = 0;
  let serverCloseTime = 0;
  let requestsReceived = 0;
  let writesReceived = 0;
  let req1Complete = false;
  let req2Error = null;

  // Handle streams on the server
  server.on('stream', (stream, headers) => {
    requestsReceived++;

    stream.respond({
      ':status': 200,
      'content-type': 'text/plain'
    });

    // Count writes from clients
    stream.on('data', (chunk) => {
      writesReceived++;
      stream.write(`Echo: ${chunk.toString().trim()}`);
    });

    stream.on('end', () => {
      stream.end('Server: Stream closed');
    });
  });

  // Start the server
  await new Promise((resolve) => server.listen(0, () => {
    serverStart = Date.now();
    resolve();
  }));
  const port = server.address().port;

  // Create client
  const client = http2.connect(`https://localhost:${port}`, {
    rejectUnauthorized: false
  });

  // Create first request that will start immediately and write 20 times eache write at interval of 100ms
  // The request will be closed at 2000ms after 20 writes
  const request1 = createClientStream(client, '1', 20, 100);

  // wait 1000ms before closing the server
  await new Promise((resolve) => setTimeout(resolve, common.platformTimeout(1000)));

  // close the server
  await new Promise((resolve) => {
    server.close(() => {
      serverCloseTime = Date.now();
      resolve();
    });
  });

  // Wait 500ms before creating the second request
  await new Promise((resolve) => setTimeout(resolve, common.platformTimeout(500)));

  // Try to create the second request after 1500ms of server start - should fail
  try {
    const request2 = createClientStream(client, '2', 10, 100);
    // If we get here without error, wait to see if an error event happens
    request2.stream.on('error', (err) => {
      req2Error = err;
    });

  } catch (err) {
    // Should fail synchronously with ERR_HTTP2_INVALID_SESSION
    req2Error = err;
  }

  // Wait for request 1 to complete gracefully (should be around 2000ms)
  await new Promise((resolve) => {
    const checkComplete = () => {
      if (!request1.isActive()) {
        req1Complete = true;
        resolve();
      } else {
        // Check again in 100ms
        setTimeout(checkComplete, common.platformTimeout(100));
      }
    };

    // Set a timeout to prevent hanging if request never completes
    setTimeout(() => {
      resolve();
    }, common.platformTimeout(1500));

    checkComplete();
  });

  // Ensure client is closed
  client.close();

  // Wait for cleanup
  await new Promise((resolve) => setTimeout(resolve, common.platformTimeout(200)));

  // Verify test expectations

  // Request 1 should have completed
  assert.ok(req1Complete, 'Request 1 should complete gracefully');
  assert.ok(request1.getWriteCount() > 0, 'Request 1 should have written data');
  // Request 1 should have written 20 times and request 2 written 0 times
  assert.strictEqual(writesReceived, 20);

  // Request 2 fails with ERR_HTTP2_INVALID_SESSION because the server
  // fired close at 1000ms which stops accepting any new request.
  // Since Request 2 starts at 1500ms, it fails.
  assert.ok(req2Error, 'Request 2 should have an error');
  // Request 2 should fail with ERR_HTTP2_INVALID_SESSION
  assert.strictEqual(req2Error.code, 'ERR_HTTP2_INVALID_SESSION');

  // Server should have received only the first request as 2nd request received after server close fired.
  assert.strictEqual(requestsReceived, 1);
  assert.ok(
    serverCloseTime - serverStart >= 2000,
    `Server should fully close after 2000ms of server start when all streams complete (actual: ${serverCloseTime - serverStart}ms)`
  );
  assert.ok(
    (serverCloseTime - serverStart) - 2000 < 200,
    `Server should fully close just after all streams complete`
  );
});
