// Flags: --experimental-quic --no-warnings

// Test: onpathvalidation callback error handling.
// A sync throw in onpathvalidation destroys the session via
// safeCallbackInvoke. The error is delivered to the onerror
// callback and the session's closed promise rejects.

import { hasQuic, skip, mustCall, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const testError = new Error('onpathvalidation throw');

// The preferred endpoint never receives a new session — it only
// routes PATH_CHALLENGE packets via SessionManager.
const preferredEndpoint = await listen(mustNotCall(), {});

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  // The server session closes with a transport error when the
  // client is destroyed by the throw. That's expected.
  await rejects(serverSession.closed, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  });
}), {
  transportParams: {
    preferredAddressIpv4: preferredEndpoint.address,
  },
});

const clientSession = await connect(serverEndpoint.address, {
  reuseEndpoint: false,
  onpathvalidation() {
    throw testError;
  },
  onerror: mustCall((err) => {
    // The error from the throw should be delivered here.
    strictEqual(err, testError);
  }),
});
await clientSession.opened;

// The session's closed should reject with the thrown error.
await rejects(clientSession.closed, testError);

await serverEndpoint.close();
await preferredEndpoint.close();
