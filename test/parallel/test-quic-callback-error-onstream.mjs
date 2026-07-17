// Flags: --experimental-quic --experimental-stream-iter --no-warnings

// Test: callback error handling for onstream.
// Sync throw in onstream destroys the session.
// safeCallbackInvoke catches the throw and calls session.destroy(error).
// The error is delivered to the onerror callback.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const encoder = new TextEncoder();

const testError = new Error('sync onstream throw');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onerror = mustCall((err) => {
    strictEqual(err, testError);
  });

  serverSession.onstream = () => {
    throw testError;
  };

  // The session's closed rejects with the error from destroy().
  await rejects(serverSession.closed, testError);
}), { transportParams: { maxIdleTimeout: 1 } });

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1 },
});
await clientSession.opened;

// Send data to trigger onstream on the server.
const stream = await clientSession.createBidirectionalStream({
  body: encoder.encode('trigger onstream'),
});

// The client session will close via CONNECTION_CLOSE or idle timeout
// after the server session is destroyed.
await Promise.all([stream.closed, clientSession.closed]);
await serverEndpoint.close();
