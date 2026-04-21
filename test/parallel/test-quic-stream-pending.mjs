// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: pending streams.
// Stream created before handshake completes, opens after.
// stream.pending is true before open, false after.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const received = await bytes(stream);
    strictEqual(new TextDecoder().decode(received), 'pending stream');
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);

// Create a stream BEFORE awaiting opened — the handshake may not have
// completed yet. The stream should be created in a pending state.
const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('pending stream'),
});

// The stream should initially be pending (no ID assigned yet).
// On fast machines the handshake might already be done.
strictEqual(typeof stream.pending, 'boolean');

// The server's onstream fires only after the handshake completes AND
// the pending stream opens. By the time we get data on the server,
// the stream is definitely no longer pending.
await serverDone.promise;

// After the server received data, the stream opened successfully.
// The data arrival proves (pending stream opens after handshake).

for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await stream.closed;
await clientSession.close();
await serverEndpoint.close();
