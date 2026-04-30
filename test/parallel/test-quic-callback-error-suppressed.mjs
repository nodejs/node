// Flags: --experimental-quic --no-warnings

// Test: SuppressedError when onerror throws.
// If session.onerror throws synchronously, a SuppressedError
//           wrapping both the onerror error and the original error is
//           thrown via process.nextTick as an uncaught exception.
// The SuppressedError's .error is the onerror failure and
//           .suppressed is the original error.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const originalError = new Error('original destroy error');
const onerrorError = new Error('onerror itself threw');

const transportParams = { maxIdleTimeout: 1 };

// The SuppressedError is thrown via process.nextTick, so it appears
// as an uncaught exception.
process.on('uncaughtException', mustCall((err) => {
  ok(err instanceof SuppressedError);
  // .error is the onerror failure
  strictEqual(err.error, onerrorError);
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

clientSession.onerror = mustCall(() => {
  throw onerrorError;
});

clientSession.destroy(originalError);

// Closed rejects with the original error (not the SuppressedError).
await rejects(clientSession.closed, originalError);
