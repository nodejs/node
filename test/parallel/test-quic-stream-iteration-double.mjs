// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: second iterator rejects when first is active.
// Because [Symbol.asyncIterator]() is an async generator, creating the
// generator always succeeds. The lock check runs inside the body, so
// the ERR_INVALID_STATE manifests as a rejected .next() promise, not
// a synchronous throw on generator creation.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    const iter1 = stream[Symbol.asyncIterator]();
    // Advance the first iterator so the lock is set.
    const first = await iter1.next();
    strictEqual(first.done, false);

    // A second iterator can be created (generator object), but
    // advancing it should reject because the lock is held.
    const iter2 = stream[Symbol.asyncIterator]();
    await rejects(iter2.next(), {
      code: 'ERR_INVALID_STATE',
    });

    // Drain the first iterator.
    for (;;) {
      const { done } = await iter1.next();
      if (done) break;
    }

    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
    serverDone.resolve();
  });
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('double iter test'),
});
for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await Promise.all([stream.closed, serverDone.promise]);
await clientSession.close();
