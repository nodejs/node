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

const { listen, connect } = await import('node:http3');
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
      stream.onheaders = mustCall((headers) => {
        // Headers were enqueued before the stream opened
        // and should arrive correctly.
        strictEqual(headers[':method'], 'GET');
        strictEqual(headers[':path'], '/pending');

        stream.sendHeaders({ ':status': '200' });
        stream.writer.writeSync(encoder.encode('ok'));
        stream.writer.endSync();
      });
      await stream.closed;
      ss.close();
      serverDone.resolve();
    });
  }), {
    sni: { '*': { keys: [key], certs: [cert] } },
  });

  const clientSession = await connect(serverEndpoint.address, {
    servername: 'localhost',
    verifyPeer: 'manual',
  });

  // Create the stream BEFORE awaiting opened. The stream is pending
  // until the handshake completes and the QUIC stream can be opened.
  const stream = await clientSession.request({
    ':method': 'GET',
    ':path': '/pending',
    ':scheme': 'https',
    ':authority': 'localhost',
  }, {
    // Priority set at creation time.
    priority: 'high',
    incremental: true,
    onheaders: mustCall(function(headers) {
      strictEqual(headers[':status'], '200');
    }),
  });

  // Priority should reflect what was set even while pending.
  deepStrictEqual(stream.priority, { level: 'high', incremental: true });

  // Reprioritize while still pending - sends deferred PRIORITY_UPDATE
  stream.setPriority({ level: 'low', incremental: false });
  deepStrictEqual(stream.priority, { level: 'low', incremental: false });

  // Now wait for the handshake.
  await clientSession.opened;

  // The reprioritized value persists after the stream opens.
  deepStrictEqual(stream.priority, { level: 'low', incremental: false });

  // Headers were sent and server responded.
  const body = await bytes(stream);
  strictEqual(decoder.decode(body), 'ok');
  await Promise.all([stream.closed, serverDone.promise]);
  await clientSession.close();
  await serverEndpoint.close();
}
