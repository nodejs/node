// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: stream limits and pending behavior.
// initialMaxStreamsBidi limits concurrent bidi streams.
//         When the limit is reached, new streams are queued as pending
//         and open when existing streams close.
// initialMaxStreamsUni limits concurrent uni streams (same behavior).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');
const { bytes } = await import('stream/iter');
const { setTimeout: sleep } = await import('timers/promises');

const encoder = new TextEncoder();
const allDone = Promise.withResolvers();
const twoDone = Promise.withResolvers();
let serverStreamCount = 0;

// Server allows only 1 bidi stream at a time.
const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    await bytes(stream);
    stream.writer.endSync();
    await stream.closed;
    ++serverStreamCount;
    if (serverStreamCount === 2) {
      twoDone.resolve();
    }
    if (serverStreamCount === 3) {
      allDone.resolve();
    }
    if (serverStreamCount === 4) {
      serverSession.close();
    }
  }, 3);
}), {
  transportParams: { initialMaxStreamsBidi: 1 },
});

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// First stream opens immediately (within the limit).
const s1 = await clientSession.createBidirectionalStream({
  body: encoder.encode('stream 1'),
  waitUntilAvailable: true
});

await assert.rejects(
  async () => {
    // Second stream should not open, but throw.
    await clientSession.createBidirectionalStream({
      body: encoder.encode('stream 2'),
      waitUntilAvailable: false,
    });
  },
  {
    name: 'Error',
    message: 'No new stream available within flow control',
  },
);

// Third stream is created but queued as pending because the
// server only allows 1 concurrent bidi stream.
const s3 = await clientSession.createBidirectionalStream({
  body: encoder.encode('stream 3'),
  waitUntilAvailable: true
});

// s3 should be pending until s1 closes and the server grants
// more stream credits.
assert.strictEqual(s3.pending, true);

// Drain and close the first stream.
for await (const _ of s1) { /* drain */ } // eslint-disable-line no-unused-vars
await s1.closed;

// After s1 closes, the server sends MAX_STREAMS which opens s3.
// Wait for the server to receive both streams.
await twoDone.promise;
// s3 should no longer be pending.
for await (const _ of s3) { /* drain */ } // eslint-disable-line no-unused-vars
await s3.closed;

await sleep(10); // We wait a bit, as we do not have a callback exposed to js
// fourth stream should open immediately and not throw
const s4 = await clientSession.createBidirectionalStream({
  body: encoder.encode('stream 4'),
  waitUntilAvailable: false
});
await Promise.all([s4.closed, allDone.promise]);

await clientSession.close();
await serverEndpoint.close();
