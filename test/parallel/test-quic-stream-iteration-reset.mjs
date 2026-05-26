// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: peer RESET_STREAM causes iterator to error.
// When the server resets the stream, the client's async iterator
// should throw or return early.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import * as assert from 'node:assert';

const { ok, rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const serverReady = Promise.withResolvers();

const serverEndpoint = await listen(mustCall((serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    // Reset the stream from the server side.
    stream.resetStream(42n);
    await rejects(stream.closed, mustCall((err) => {
      assert.ok(err);
      return true;
    }));
    serverReady.resolve();
    await serverSession.closed;
  });
}), { transportParams: { maxIdleTimeout: 1 } });

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1 },
});
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('will be reset by server'),
});

// Set up the closed handler before the reset to avoid unhandled rejection.
const closedPromise = rejects(stream.closed, mustCall((err) => {
  assert.ok(err);
  return true;
}));

await serverReady.promise;

// The async iterator should either throw or return early when the
// peer resets the readable side.
try {
  for await (const batch of stream) {
    // May receive some data before the reset arrives.
    ok(Array.isArray(batch));
  }
} catch {
  // The iterator may throw when the reset arrives mid-iteration.
}

// Either way, the stream should close.
await closedPromise;
await clientSession.closed;
await serverEndpoint.close();
