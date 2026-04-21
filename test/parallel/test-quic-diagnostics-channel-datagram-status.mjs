// Flags: --experimental-quic --no-warnings

// Test: diagnostics_channel datagram status event.
// quic.session.receive.datagram.status fires with ack/lost status.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const statusDone = Promise.withResolvers();

// quic.session.receive.datagram.status fires with status.
dc.subscribe('quic.session.receive.datagram.status', mustCall((msg) => {
  ok(msg.session);
  ok(msg.id);
  strictEqual(msg?.status, 'acknowledged');
  statusDone.resolve();
}));

const serverEndpoint = await listen(async (serverSession) => {
  // Server stays alive until the client closes so the ACK
  // has time to propagate back to the client.
  await serverSession.closed;
}, {
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagram() {},
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagramstatus() {},
});
await clientSession.opened;

await clientSession.sendDatagram(new Uint8Array([1, 2, 3]));

// Wait for the status event before closing.
await statusDone.promise;
await clientSession.close();
await serverEndpoint.close();
