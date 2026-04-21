// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: async rejection in onstream destroys session.
// safeCallbackInvoke detects the returned promise and attaches a
// rejection handler that calls session.destroy(err). The error is
// delivered to the onerror callback.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual, rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const testError = new Error('async onstream rejection');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onerror = mustCall((err) => {
    strictEqual(err, testError);
  });

  serverSession.onstream = async () => {
    throw testError;
  };

  // Session closed rejects with the error from the async rejection.
  await rejects(serverSession.closed, testError);
}), { transportParams: { maxIdleTimeout: 1 } });

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1 },
});
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream({
  body: new TextEncoder().encode('trigger onstream'),
});

// The client session closes via CONNECTION_CLOSE or idle timeout
// after the server session is destroyed by the async rejection.
await Promise.all([stream.closed, clientSession.closed]);
await serverEndpoint.close();
