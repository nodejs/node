// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: non-readable stream returns finished iterator.
// The sender side of a unidirectional stream is not readable, so
// iterating it should immediately return done: true.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

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
    await bytes(stream);
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createUnidirectionalStream({
  body: encoder.encode('uni data'),
});

// The sender side of a uni stream is not readable.
const iter = stream[Symbol.asyncIterator]();
const { done } = await iter.next();
strictEqual(done, true);

await Promise.all([serverDone.promise, stream.closed]);
await clientSession.close();
await serverEndpoint.close();
