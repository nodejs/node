// Flags: --experimental-quic --no-warnings

// Test: active session sends CONNECTION_CLOSE when closing.
// When the server calls session.close() on an active session, the peer
// receives a CONNECTION_CLOSE frame with NO_ERROR. The client's closed
// promise resolves (clean close), rather than rejecting with an idle
// timeout or transport error.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverReady = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  // Signal to the client that the server is ready, then close.
  serverReady.resolve();
  await serverSession.close();
}));

const clientSession = await connect(serverEndpoint.address, {
  reuseEndpoint: false,
});

await Promise.all([clientSession.opened, serverReady.promise]);

// The client receives CONNECTION_CLOSE with NO_ERROR.
// The closed promise should resolve (not reject). If the server
// failed to send CONNECTION_CLOSE (e.g., used silent close or
// stateless reset), the client would time out and closed would
// reject with ERR_QUIC_TRANSPORT_ERROR.
await clientSession.closed;

// If we reach here, the session closed cleanly — CONNECTION_CLOSE
// was received, not an idle timeout.
assert.ok(clientSession.destroyed, 'session should be destroyed');

await serverEndpoint.close();
