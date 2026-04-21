// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: HTTP/3 GOAWAY handling.
// Graceful close sends GOAWAY - client receives ongoaway callback
// After GOAWAY, new stream creation fails
// Existing streams continue and complete after GOAWAY
// Opens two concurrent streams. Server responds to the first immediately
// and holds the second response. The server session.close() is called from
// the main test body (not a callback) after the client confirms both
// streams' headers were received. The second stream is still active,
// ensuring the GOAWAY is sent separately from CONNECTION_CLOSE.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';
import * as fixtures from '../common/fixtures.mjs';
const { readKey } = fixtures;

const { ok, strictEqual, rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');
const { bytes } = await import('stream/iter');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');
const encoder = new TextEncoder();
const decoder = new TextDecoder();

// quic.session.goaway fires when the peer sends GOAWAY.
dc.subscribe('quic.session.goaway', mustCall((msg) => {
  ok(msg.session, 'goaway should include session');
  strictEqual(typeof msg.lastStreamId, 'bigint', 'goaway should include lastStreamId');
}));

{
  let serverSession;
  let pendingSecondStream;
  const goawayReceived = Promise.withResolvers();
  const completeSecondResponse = Promise.withResolvers();
  const bothHeadersReceived = Promise.withResolvers();
  let clientHeaderCount = 0;

  const serverEndpoint = await listen(mustCall(async (ss) => {
    serverSession = ss;
    ss.onstream = mustCall(2);
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    onheaders: mustCall(function(headers) {
      const path = headers[':path'];
      this.sendHeaders({ ':status': '200' });

      if (path === '/first') {
        // Respond immediately to the first request.
        this.writer.writeSync(encoder.encode('first'));
        this.writer.endSync();
      } else if (path === '/second') {
        // Hold the second response until signaled.
        pendingSecondStream = this;
        completeSecondResponse.promise.then(mustCall(() => {
          pendingSecondStream.writer.writeSync(encoder.encode('second'));
          pendingSecondStream.writer.endSync();
        }));
      }
    }, 2),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    // Ongoaway fires when the peer sends GOAWAY.
    ongoaway: mustCall(function(lastStreamId) {
      strictEqual(lastStreamId, -1n);
      goawayReceived.resolve();
    }),
  });
  await clientSession.opened;

  const onClientHeaders = mustCall(function(headers) {
    strictEqual(headers[':status'], '200');
    if (++clientHeaderCount === 2) {
      bothHeadersReceived.resolve();
    }
  }, 2);

  const stream1 = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/first',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: onClientHeaders,
  });

  const stream2 = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/second',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    onheaders: onClientHeaders,
  });

  // First stream completes immediately.
  const body1 = await bytes(stream1);
  strictEqual(decoder.decode(body1), 'first');

  // Wait for both streams' headers to arrive on the client, confirming
  // the server has processed both requests.
  await bothHeadersReceived.promise;

  // Close the server session from the main test body. The second
  // stream's body is still pending, so the graceful close sends
  // GOAWAY (shutdown notice) separately from CONNECTION_CLOSE.
  serverSession.close();

  // Wait for GOAWAY notification on the client.
  await goawayReceived.promise;

  // After GOAWAY, new stream creation should fail.
  await rejects(
    clientSession.createBidirectionalStream({
      headers: {
        ':method': 'GET',
        ':path': '/new',
        ':scheme': 'https',
        ':authority': 'localhost',
      },
    }),
    { code: 'ERR_INVALID_STATE' },
  );

  // Signal the server to complete the second response.
  completeSecondResponse.resolve();

  // Second stream also completes despite GOAWAY.
  const body2 = await bytes(stream2);
  strictEqual(decoder.decode(body2), 'second');

  // Both streams close cleanly.
  await Promise.all([stream1.closed, stream2.closed]);
  clientSession.close();
}
