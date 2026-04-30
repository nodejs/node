// Flags: --experimental-quic --no-warnings

// Test: datagram size limit enforcement.
// Datagram larger than maxDatagramSize returns 0n (not sent).
// Datagram at exactly maxDatagramSize is accepted and delivered.
// Same as DGRAM-03 via sendDatagram return value.
// maxDatagramSize reflects the maximum datagram payload the peer can
// receive, accounting for DATAGRAM frame overhead (type byte + varint
// length encoding). It is derived from the peer's maxDatagramFrameSize
// transport parameter minus the frame overhead.
// We use maxDatagramFrameSize: 200 so that the exact-max datagram fits
// comfortably within a QUIC packet (which has its own header + AEAD
// overhead on top of the DATAGRAM frame).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok, strictEqual, notStrictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverGot = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverGot.promise;
  serverSession.close();
  await serverSession.closed;
}), {
  transportParams: { maxDatagramFrameSize: 200 },
  ondatagram: mustCall((data) => {
    ok(data instanceof Uint8Array);
    serverGot.resolve();
  }),
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxDatagramFrameSize: 200 },
});
await clientSession.opened;

const maxSize = clientSession.maxDatagramSize;

// maxDatagramSize should be less than maxDatagramFrameSize due to
// the DATAGRAM frame overhead (1 byte type + varint length encoding).
ok(maxSize > 0);
ok(maxSize < 200);

// DGRAM-03 / DGIMP-10: Datagram too large — returns 0n.
const oversized = new Uint8Array(maxSize + 1);
const tooLargeId = await clientSession.sendDatagram(oversized);
strictEqual(tooLargeId, 0n);

// Datagram at exactly maxDatagramSize — accepted and delivered.
const exactMax = new Uint8Array(maxSize);
exactMax[0] = 42;
const exactId = await clientSession.sendDatagram(exactMax);
notStrictEqual(exactId, 0n);

await Promise.all([serverGot.promise, clientSession.closed]);
await serverEndpoint.close();
