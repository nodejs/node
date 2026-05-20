// Flags: --experimental-quic --no-warnings

// Test: onsessionticket callback error handling.
// A sync throw in onsessionticket destroys the session via safeCallbackInvoke.
// Unlike onhandshake throws, no uncaughtException is produced because the
// session ticket arrives after the handshake completes (session.opened is
// already resolved so there is no unhandled rejection).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const testError = new Error('onsessionticket throw');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}), { transportParams: { maxIdleTimeout: 1 } });

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1 },
  onsessionticket() {
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
