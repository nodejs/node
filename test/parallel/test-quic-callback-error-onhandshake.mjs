// Flags: --experimental-quic --no-warnings

// Test: onhandshake callback error handling.
// A sync throw in onhandshake destroys the session via safeCallbackInvoke.
// The error is delivered to the onerror callback and the session's
// closed promise rejects with the error.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const testError = new Error('onhandshake throw');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}), { transportParams: { maxIdleTimeout: 1 } });

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1 },
  onhandshake() {
    throw testError;
  },
  onerror: mustCall((err) => {
    assert.strictEqual(err, testError);
  }),
});

// The session's closed should reject with the error from the throw.
await assert.rejects(clientSession.closed, testError);

await serverEndpoint.close();
