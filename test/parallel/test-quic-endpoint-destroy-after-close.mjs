// Flags: --experimental-quic --no-warnings

// Test: When `endpoint.destroy(err)` is called *after* `endpoint.close()`
// has already initiated graceful shutdown, the error must still surface
// on `endpoint.closed` instead of being swallowed. First-error-wins
// semantics: a fatal error reported via `destroy(err)` after a
// graceful close was already in flight is still propagated through
// `endpoint.closed`.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// Hold the connection open for a while so that close() has actual work
// to wait on.
const transportParams = { maxIdleTimeout: 5000 };

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  // Don't observe `serverSession.closed` directly here - the cascade
  // from `endpoint.destroy(err)` will reject it, and the outer
  // `markPromiseAsHandled` in the cascade keeps that from surfacing as
  // an unhandled rejection. We just need this callback to keep running
  // until the test ends.
  try {
    await serverSession.closed;
  } catch {
    // Expected: the cascade destroys the session with an error.
  }
  serverDone.resolve();
}), { transportParams });

const clientSession = await connect(serverEndpoint.address, {
  transportParams,
});
await clientSession.opened;

// Step 1: kick off a graceful close. After this, the endpoint is in
// the "closing" state - so `#isClosedOrClosing` is true and the
// pre-fix `destroy()` would skip recording `#pendingError`.
const closingPromise = serverEndpoint.close();

// Step 2: while the graceful close is in flight, call destroy(err).
// With the fix, the error is still recorded and surfaces on
// `endpoint.closed`. Without the fix, it'd be silently dropped.
const destroyError = new Error('destroy after close');
const closedAssertion = rejects(serverEndpoint.closed, destroyError);

serverEndpoint.destroy(destroyError);

await closedAssertion;

// `endpoint.close()` returns the same promise as `endpoint.closed`, so
// it should reject with the same error as well.
await rejects(closingPromise, destroyError);

await serverDone.promise;
clientSession.destroy();
