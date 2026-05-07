// Flags: --experimental-quic --no-warnings

// Test: diagnostics_channel datagram events.
// quic.session.receive.datagram fires on datagram receipt.
// quic.session.send.datagram fires on datagram send.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';

const { ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverGot = Promise.withResolvers();

// quic.session.send.datagram fires on send.
dc.subscribe('quic.session.send.datagram', mustCall((msg) => {
  ok(msg.session);
  ok(msg.id);
  ok(msg.length > 0);
}));

// quic.session.receive.datagram fires on receipt.
dc.subscribe('quic.session.receive.datagram', mustCall((msg) => {
  ok(msg.session);
  ok(msg.length > 0);
}));

const serverEndpoint = await listen(async (serverSession) => {
  await serverSession.closed;
}, {
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagram() {
    serverGot.resolve();
  },
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxDatagramFrameSize: 1200 },
});
await clientSession.opened;

await clientSession.sendDatagram(new Uint8Array([1, 2, 3]));

await serverGot.promise;
await clientSession.close();
await serverEndpoint.close();
