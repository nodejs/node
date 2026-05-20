// Flags: --experimental-quic --no-warnings

// Test: onerror set via connect() options.
// The onerror callback can be provided in the options object at
// session creation time to avoid race conditions with errors that
// occur during or immediately after the handshake.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const transportParams = { maxIdleTimeout: 1 };
const testError = new Error('destroy with error');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}), { transportParams });

const clientSession = await connect(serverEndpoint.address, {
  transportParams,
  onerror: mustCall((err) => {
    strictEqual(err, testError);
  }),
});
await clientSession.opened;

clientSession.destroy(testError);

await rejects(clientSession.closed, testError);
