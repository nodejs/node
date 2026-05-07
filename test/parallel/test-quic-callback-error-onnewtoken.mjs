// Flags: --experimental-quic --no-warnings

// Test: onnewtoken callback error handling.
// A sync throw in onnewtoken destroys the session via safeCallbackInvoke.
// The server submits a NEW_TOKEN after handshake completes; the client
// receives it via the onnewtoken callback. Since the session ticket and
// NEW_TOKEN both arrive after the handshake, session.opened is already
// resolved and there is no unhandled rejection / uncaught exception.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const testError = new Error('onnewtoken throw');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}), { transportParams: { maxIdleTimeout: 1 } });

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1 },
  onnewtoken() {
    throw testError;
  },
  onerror: mustCall((err) => {
    strictEqual(err, testError);
  }),
});

await clientSession.opened;

// The session's closed should reject with the error from the throw.
await rejects(clientSession.closed, testError);

await serverEndpoint.close();
