// Flags: --experimental-quic --no-warnings

// session.sendDatagram() must not detach the caller's underlying
// ArrayBuffer. The bytes are copied into an internal buffer on every
// send, so the caller's source ArrayBuffer remains live and may be
// reused for further sends, mutated, or sliced into additional views.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual, deepStrictEqual, notStrictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const received = [];
const allReceived = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await allReceived.promise;
  await serverSession.close();
}), {
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagram: mustCall((data) => {
    received.push(Buffer.from(data));
    if (received.length === 3) allReceived.resolve();
  }, 3),
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxDatagramFrameSize: 1200 },
});
await clientSession.opened;

// A full-buffer Uint8Array. The same source must be reusable across
// multiple sendDatagram calls.
const source = new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8]);

const firstId = await clientSession.sendDatagram(source);
notStrictEqual(firstId, 0n);
strictEqual(source.buffer.detached, false,
            'source ArrayBuffer must not be detached after sendDatagram');

const secondId = await clientSession.sendDatagram(source);
notStrictEqual(secondId, 0n);
strictEqual(source.buffer.detached, false,
            'source ArrayBuffer must remain live after second sendDatagram');

// Mutating the source after the previous sendDatagram returned must
// not affect what the peer ultimately receives — the bytes have
// already been copied into the QUIC layer's internal buffer.
source[0] = 99;
const thirdId = await clientSession.sendDatagram(source);
notStrictEqual(thirdId, 0n);

await allReceived.promise;

// Datagrams are unreliable and may be reordered, but the test fixture
// does not exercise loss or reordering — assert by sorting.
const sorted = received.map((b) => b.toString('hex')).sort();
const expected = [
  Buffer.from([1, 2, 3, 4, 5, 6, 7, 8]).toString('hex'),
  Buffer.from([1, 2, 3, 4, 5, 6, 7, 8]).toString('hex'),
  Buffer.from([99, 2, 3, 4, 5, 6, 7, 8]).toString('hex'),
].sort();
deepStrictEqual(sorted, expected);

await clientSession.closed;
await serverEndpoint.close();
