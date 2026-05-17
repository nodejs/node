// Flags: --experimental-quic --no-warnings

// Test: ondatagramstatus callback error handling.
// A sync throw in ondatagramstatus destroys the session via safeCallbackInvoke.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const testError = new Error('ondatagramstatus throw');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}), {
  transportParams: { maxIdleTimeout: 1, maxDatagramFrameSize: 1200 },
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1, maxDatagramFrameSize: 1200 },
  ondatagramstatus() {
    throw testError;
  },
  onerror: mustCall((err) => {
    strictEqual(err, testError);
  }),
});
await clientSession.opened;

// Send a datagram. The status callback fires when the peer ACKs it.
await clientSession.sendDatagram(new Uint8Array([1, 2, 3]));

// The session's closed should reject with the error from the throw.
await rejects(clientSession.closed, testError);
