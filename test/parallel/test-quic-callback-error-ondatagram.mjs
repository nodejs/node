// Flags: --experimental-quic --no-warnings

// Test: ondatagram callback error handling.
// A sync throw in ondatagram destroys the session via safeCallbackInvoke.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const testError = new Error('ondatagram throw');
const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  // The session is destroyed by the ondatagram throw. The closed promise
  // rejects with testError. Verify that and signal completion.
  await rejects(serverSession.closed, testError);
  serverDone.resolve();
}), {
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagram() {
    throw testError;
  },
  onerror: mustCall((err) => {
    strictEqual(err, testError);
  }),
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1, maxDatagramFrameSize: 1200 },
});
await clientSession.opened;

// Send a datagram to trigger the server's ondatagram callback.
await clientSession.sendDatagram(new Uint8Array([1, 2, 3]));

await serverDone.promise;
// The server session was destroyed abruptly (no CONNECTION_CLOSE sent).
// The client may receive a stateless reset if it sends any packet
// before its idle timeout fires, so closed may reject.
await rejects(clientSession.closed, { code: 'ERR_QUIC_TRANSPORT_ERROR' });
await serverEndpoint.close();
