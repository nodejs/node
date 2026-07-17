// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stream limits and pending behavior.
// initialMaxStreamsBidi limits concurrent bidi streams.
//         When the limit is reached, new streams are queued as pending
//         and open when existing streams close.
// initialMaxStreamsUni limits concurrent uni streams (same behavior).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');

const encoder = new TextEncoder();
const allDone = Promise.withResolvers();
let serverStreamCount = 0;

// Server allows only 1 bidi stream at a time.
const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    await bytes(stream);
    stream.writer.endSync();
    await stream.closed;
    if (++serverStreamCount === 2) {
      serverSession.close();
      allDone.resolve();
    }
  }, 2);
}), {
  transportParams: { initialMaxStreamsBidi: 1 },
});

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// First stream opens immediately (within the limit).
const s1 = await clientSession.createBidirectionalStream({
  body: encoder.encode('stream 1'),
});

// Second stream is created but queued as pending because the
// server only allows 1 concurrent bidi stream.
const s2 = await clientSession.createBidirectionalStream({
  body: encoder.encode('stream 2'),
});

// s2 should be pending until s1 closes and the server grants
// more stream credits.
strictEqual(s2.pending, true);

// Drain and close the first stream.
for await (const _ of s1) { /* drain */ } // eslint-disable-line no-unused-vars
await s1.closed;

// After s1 closes, the server sends MAX_STREAMS which opens s2.
// Wait for the server to receive both streams.
await allDone.promise;

// s2 should no longer be pending.
for await (const _ of s2) { /* drain */ } // eslint-disable-line no-unused-vars
await s2.closed;

await clientSession.close();
await serverEndpoint.close();
