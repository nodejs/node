// Flags: --experimental-quic --no-warnings

// Test: Symbol.asyncDispose for endpoint and session.
// endpoint[Symbol.asyncDispose] closes the endpoint.
// session[Symbol.asyncDispose] closes the session.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  // Wait for the session to close (triggered by the client's close).
  await serverSession.closed;
  serverDone.resolve();
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// session[Symbol.asyncDispose] closes the session.
strictEqual(typeof clientSession[Symbol.asyncDispose], 'function');
await clientSession[Symbol.asyncDispose]();
strictEqual(clientSession.destroyed, true);

await serverDone.promise;

// endpoint[Symbol.asyncDispose] closes the endpoint.
strictEqual(typeof serverEndpoint[Symbol.asyncDispose], 'function');
await serverEndpoint[Symbol.asyncDispose]();
strictEqual(serverEndpoint.destroyed, true);
