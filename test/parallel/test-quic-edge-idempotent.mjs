// Flags: --experimental-quic --no-warnings

// Test: double close/destroy are idempotent.
// Double close() on session is idempotent.
// Double close() on endpoint is idempotent.
// Double destroy() on session is idempotent.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  serverSession.onstream = mustCall(async (stream) => {
    stream.writer.endSync();
    await stream.closed;
    serverSession.close();
  });
  await serverSession.closed;
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Signal server to close via stream first (before we close).
const stream = await clientSession.createBidirectionalStream();
stream.writer.endSync();
for await (const _ of stream) { /* drain */ } // eslint-disable-line no-unused-vars
await stream.closed;

// Double close() on session — both return the same promise.
const p1 = clientSession.close();
const p2 = clientSession.close();
strictEqual(p1, p2);
await clientSession.closed;

// Double destroy() — second call is no-op.
clientSession.destroy();
strictEqual(clientSession.destroyed, true);
clientSession.destroy();  // Should not throw.
strictEqual(clientSession.destroyed, true);

// Double close() on endpoint.
const ep1 = serverEndpoint.close();
const ep2 = serverEndpoint.close();
strictEqual(ep1, ep2);
await serverEndpoint.closed;
