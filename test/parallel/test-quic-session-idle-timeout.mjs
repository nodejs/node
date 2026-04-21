// Flags: --experimental-quic --no-warnings

// Test: idle timeout closes the session.
// Both client and server are configured with a short maxIdleTimeout.
// After the handshake completes, neither side sends any data. The idle
// timeout fires and both sessions close without error.

import { hasQuic, skip, mustCall } from '../common/index.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const transportParams = { maxIdleTimeout: 1 };  // 1 second

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  // The server's closed promise should resolve when the idle timeout fires.
  await serverSession.closed;
  serverDone.resolve();
}), {
  transportParams,
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams,
});

await clientSession.opened;

// Don't send anything. Just wait for the idle timeout to close the session.
await Promise.all([clientSession.closed, serverDone.promise]);

await serverEndpoint.close();
