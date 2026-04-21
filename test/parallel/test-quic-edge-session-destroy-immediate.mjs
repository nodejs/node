// Flags: --experimental-quic --no-warnings

// Test: session created and immediately destroyed.
// Calling destroy() on a session that hasn't completed handshake should
// not crash. The opened and closed promises should settle appropriately.

import { hasQuic, skip, mustNotCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// The client destroys before handshake completes, so the server
// should never see a session.
const serverEndpoint = await listen(mustNotCall(), {
  transportParams: { maxIdleTimeout: 1 },
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1 },
});

// Destroy immediately without waiting for opened.
clientSession.destroy();

strictEqual(clientSession.destroyed, true);

// Opened may reject (session destroyed before handshake completed)
// or resolve if handshake completed fast enough.
// Closed should resolve (destroy without error).
await clientSession.closed;
await serverEndpoint.close();
