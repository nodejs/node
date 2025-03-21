'use strict';
const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); };
const fixtures = require('../common/fixtures');
// This test ensure that server will close grcaefully after completing all active streams.
// It also ensures that idle connections will close immediately after server close is called.
const assert = require('assert');
const http2 = require('http2');

const { test } = require('node:test');

/**
 * Create and manage an HTTP/2 client with a single stream
 * @param {http2.ClientHttp2Session} client - The HTTP/2 client session
 * @param {string} clientId - Identifier for the client (e.g., '1', '2')
 * @param {number} writeCount - Number of writes to perform
 * @param {number} writeInterval - Interval between writes in ms
 * @returns {object} - Object containing stream, writeTracker, and cleanup function
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

// This test init a server and connect two clients, client1 and client2.
// Client1 sends 10 writes at interval of 100ms and then close at 1000ms from server start.
// Client2 sends 20 writes at interval of 100ms and then close at 2000ms from server start.
// Server close is fired after 1500ms from server start.
// This test verifies that server closes all active sessions gracefully and
// client1 session terminates immediately after server close is fired as server immediately closes all idle sessions.
// client2 session terminates approximately 500ms after server close as client2 write till 2000ms from server start.
test('HTTP/2 server close behavior with different client behavior', async () => {
  const server = http2.createSecureServer({
    key: fixtures.readKey('agent1-key.pem'),
    cert: fixtures.readKey('agent1-cert.pem'),
  });

  let serverStart = 0;
  let streamCount = 0;
  let writeCount1 = 0;
  let writeCount2 = 0;
  let serverCloseTime = 0;

  // Track server-side session termination
  const sessionTerminationTimes = new Map();

  server.on('session', (session) => {

    // Store the client ID in the session for identification
    session.clientId = null;

    session.on('close', () => {
      const terminationTime = Date.now();
      if (session.clientId) {
        sessionTerminationTimes.set(session.clientId, terminationTime);
      }
    });

    session.on('error', (err) => {
      console.error(`Session error:`, err);
    });
  });

  // Server stream handler
  server.on('stream', (stream, headers) => {
    const clientId = headers['client-id'];
    streamCount++;

    // Identify the session this stream belongs to
    if (stream.session && !stream.session.clientId) {
      stream.session.clientId = clientId;
    }

    stream.respond({
      ':status': 200,
      'content-type': 'text/plain'
    });

    // Handle incoming data
    stream.on('data', (chunk) => {
      const data = chunk.toString().trim();

      if (clientId === '1') {
        writeCount1++;
      } else if (clientId === '2') {
        writeCount2++;
      }

      stream.write(`Echo: ${data}`);
    });

    stream.on('end', () => {});

    stream.on('error', (err) => {
      console.error(`Stream ${stream.id} error:`, err);
    });
  });

  // Start the server
  await new Promise((resolve) => server.listen(0, () => {
    serverStart = Date.now();
    resolve();
  }));

  const port = server.address().port;

  // This client will send 10 writes at interval of 100ms and then close at 1000ms from server start.
  const client1 = http2.connect(`https://localhost:${port}`, {
    rejectUnauthorized: false
  });

  // This client will send 20 writes at interval of 100ms and then close at 2000ms from server start.
  const client2 = http2.connect(`https://localhost:${port}`, {
    rejectUnauthorized: false
  });

  // Create streams for both clients
  createClientStream(client1, '1', 10, 100);
  createClientStream(client2, '2', 20, 100);


  // Wait for 1500ms before server close
  await new Promise((resolve) => setTimeout(resolve, common.platformTimeout(1500)));
  const serverCloseFireTime = Date.now();

  // Fire server close after 1500ms of server start
  await new Promise((resolve) => {
    server.close(() => {
      serverCloseTime = Date.now();
      resolve();
    });
  });


  // Wait for all server-side session terminations to complete
  await new Promise((resolve) => setTimeout(resolve, common.platformTimeout(500)));

  // Verify test expectations
  // Should have exactly 2 streams (one per client)'
  assert.strictEqual(streamCount, 2);
  // Server should have received 10 writes from client 1
  assert.strictEqual(writeCount1, 10);
  // Server should have received 20 writes from client 2
  assert.strictEqual(writeCount2, 20);

  // Verify session termination times from server's perspective
  assert.ok(sessionTerminationTimes.has('1'), 'Server should record termination of client 1 session');
  assert.ok(sessionTerminationTimes.has('2'), 'Server should record termination of client 2 session');

  const client1TerminationTime = sessionTerminationTimes.get('1');
  const client2TerminationTime = sessionTerminationTimes.get('2');

  // Client 1 session should terminate immediately after
  // server close is fired as server immediately closes all idle sessions.
  assert.ok(
    client1TerminationTime - serverCloseTime < 100,
    `Client 1 session should terminate shortly after server close (actual delay: ${client1TerminationTime - serverCloseTime}ms)`
  );

  // Client 2 session should terminate approximately 500ms after
  // server close as client  write till 2000ms from server start.
  const client2Delay = client2TerminationTime - serverCloseFireTime;
  assert.ok(
    client2Delay >= 450 && client2Delay <= 650,
    `Client 2 session should terminate ~500ms after server close (actual delay: ${client2Delay}ms)`
  );
  assert.ok(
    serverCloseTime - serverStart >= 2000,
    `Server should fully close after 2000ms of server start when all streams complete (actual: ${serverCloseTime - serverStart}ms)`
  );
  assert.ok(
    (serverCloseTime - serverStart) - 2000 < 200,
    `Server should fully close just after all streams complete`
  );

});
