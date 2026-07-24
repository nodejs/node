// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: session stats increment with data transfer and track streams
// bytesReceived/bytesSent increment after data transfer.
// bidiInStreamCount/bidiOutStreamCount track streams.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const payload = encoder.encode('hello stats world');
const payloadLength = payload.byteLength;
const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const data = await bytes(stream);
    strictEqual(data.byteLength, payloadLength);
    stream.writer.endSync();
    await stream.closed;

    // Server sees one inbound bidi stream.
    strictEqual(serverSession.stats.bidiInStreamCount, 1n);
    strictEqual(serverSession.stats.bidiOutStreamCount, 0n);

    // Server received data bytes.
    ok(serverSession.stats.bytesReceived > 0n);
    ok(serverSession.stats.bytesSent > 0n);

    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Before sending, bytes should be from handshake only.
const bytesSentBefore = clientSession.stats.bytesSent;
ok(bytesSentBefore > 0n, 'handshake bytes should be counted');

const stream = await clientSession.createBidirectionalStream({
  body: payload,
});

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await stream.closed;
await serverDone.promise;

// After sending, bytesSent should have increased.
ok(clientSession.stats.bytesSent > bytesSentBefore,
   'bytesSent should increase after data transfer');
ok(clientSession.stats.bytesReceived > 0n);

// Client opened one outbound bidi stream.
strictEqual(clientSession.stats.bidiOutStreamCount, 1n);
strictEqual(clientSession.stats.bidiInStreamCount, 0n);

// Verify RTT fields are populated (connection was active).
ok(clientSession.stats.smoothedRtt > 0n);

await clientSession.closed;
await serverEndpoint.close();
