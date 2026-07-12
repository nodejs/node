// Flags: --experimental-quic --no-warnings

// Test: session created and immediately closed.
// Calling close() on a session right after creation (before handshake
// completes) should gracefully close the session without crashing.

import { hasQuic, skip } from '../common/index.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverEndpoint = await listen(async (serverSession) => {
  await serverSession.closed;
}, {
  transportParams: { maxIdleTimeout: 1 },
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1 },
});

// Close immediately without waiting for opened.
await clientSession.close();
await serverEndpoint.close();
