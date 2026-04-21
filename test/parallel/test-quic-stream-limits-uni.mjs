// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: uni stream limits and pending behavior.
// initialMaxStreamsUni = 1 limits concurrent uni streams. The second
// stream is queued as pending and opens after the first closes.

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

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    await bytes(stream);
    await stream.closed;
    if (++serverStreamCount === 2) {
      serverSession.close();
      allDone.resolve();
    }
  }, 2);
}), {
  transportParams: { initialMaxStreamsUni: 1 },
});

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// First uni stream opens immediately.
const s1 = await clientSession.createUnidirectionalStream({
  body: encoder.encode('uni 1'),
});

// Second uni stream is pending (limit = 1).
const s2 = await clientSession.createUnidirectionalStream({
  body: encoder.encode('uni 2'),
});
strictEqual(s2.pending, true);

// Wait for both to complete.
await s1.closed;
await allDone.promise;
await s2.closed;

await clientSession.close();
await serverEndpoint.close();
