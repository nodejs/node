// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: Pending H3 stream behavior.
// Priority set at creation time is applied to pending stream
// Headers enqueued at creation time are sent when stream opens

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { strictEqual, deepStrictEqual } = assert;
const { readKey } = fixtures;

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

// The stream is initially pending (waiting for the QUIC handshake
// to open it). Priority and headers should be applied when it opens.
{
  const serverDone = Promise.withResolvers();

  const serverEndpoint = await listen(mustCall(async (ss) => {
    ss.onstream = mustCall(async (stream) => {
      await stream.closed;
      ss.close();
      serverDone.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
    onheaders: mustCall(function(headers) {
      // Headers were enqueued before the stream opened
      // and should arrive correctly.
      strictEqual(headers[':method'], 'GET');
      strictEqual(headers[':path'], '/pending');

      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync(encoder.encode('ok'));
      this.writer.endSync();
    }),
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
  });

  // Create the stream BEFORE awaiting opened. The stream is pending
  // until the handshake completes and the QUIC stream can be opened.
  const stream = await clientSession.createBidirectionalStream({
    headers: {
      ':method': 'GET',
      ':path': '/pending',
      ':scheme': 'https',
      ':authority': 'localhost',
    },
    // Priority set at creation time.
    priority: 'high',
    incremental: true,
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
    }),
  });

  // Priority should reflect what was set even while pending.
  deepStrictEqual(stream.priority, { level: 'high', incremental: true });

  // Now wait for the handshake.
  await clientSession.opened;

  // Priority persists after stream opens.
  deepStrictEqual(stream.priority, { level: 'high', incremental: true });

  // Headers were sent and server responded.
  const body = await bytes(stream);
  strictEqual(decoder.decode(body), 'ok');
  await Promise.all([stream.closed, serverDone.promise]);
  clientSession.close();
}
