// Flags: --experimental-quic --no-warnings

// Test: handshake timeout.
// The server accepts sessions but the client uses a very short idle
// timeout, causing the session to close before the handshake can
// complete on the server side.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}), { transportParams: { maxIdleTimeout: 1 } });

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1 },
});

// Don't send any data. Just wait for idle timeout.
await Promise.all([clientSession.opened, clientSession.closed]);

// The session closed via idle timeout. Verify it was destroyed.
strictEqual(clientSession.destroyed, true);

await serverEndpoint.close();
