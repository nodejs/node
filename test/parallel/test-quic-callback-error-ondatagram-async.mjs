// Flags: --experimental-quic --no-warnings

// Test: async rejection in ondatagram destroys session.
// safeCallbackInvoke detects the returned promise and attaches a
// rejection handler that calls session.destroy(err). The error is
// delivered to the onerror callback.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const testError = new Error('async ondatagram rejection');
const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await assert.rejects(serverSession.closed, testError);
  serverDone.resolve();
}), {
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagram: mustCall(async () => {
    throw testError;
  }),
  onerror: mustCall((err) => {
    assert.strictEqual(err, testError);
  }),
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxIdleTimeout: 1, maxDatagramFrameSize: 1200 },
});
await clientSession.opened;

await clientSession.sendDatagram(new Uint8Array([1, 2, 3]));

await serverDone.promise;
// The server session was destroyed abruptly (no CONNECTION_CLOSE sent).
// The client may receive a stateless reset if it sends any packet
// before its idle timeout fires, so closed may reject.
await assert.rejects(clientSession.closed, { code: 'ERR_QUIC_TRANSPORT_ERROR' });
serverEndpoint.close();
await serverEndpoint.closed;
