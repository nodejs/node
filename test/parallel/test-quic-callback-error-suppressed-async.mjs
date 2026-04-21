// Flags: --experimental-quic --no-warnings

// Test: SuppressedError when async onerror rejects.
// When session.onerror returns a Promise that rejects, a SuppressedError
// wrapping both the rejection reason and the original error is thrown
// via process.nextTick as an uncaught exception.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const originalError = new Error('original destroy error');
const onerrorRejection = new Error('async onerror rejected');

const transportParams = { maxIdleTimeout: 1 };

// The SuppressedError is thrown via process.nextTick after the
// onerror promise rejects, so it appears as an uncaught exception.
process.on('uncaughtException', mustCall((err) => {
  ok(err instanceof SuppressedError);
  // .error is the onerror rejection reason
  strictEqual(err.error, onerrorRejection);
  // .suppressed is the original error that triggered destroy
  strictEqual(err.suppressed, originalError);
}));

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}), { transportParams });

const clientSession = await connect(serverEndpoint.address, {
  transportParams,
});
await clientSession.opened;

// Async onerror: returns a promise that rejects.
clientSession.onerror = mustCall(async () => {
  throw onerrorRejection;
});

clientSession.destroy(originalError);

// Closed rejects with the original error (not the SuppressedError).
await rejects(clientSession.closed, originalError);

await serverEndpoint.close();
