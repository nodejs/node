// Flags: --experimental-quic --no-warnings

// Test: datagram abandoned status for queue overflow.
// When the datagram pending queue is full and a new datagram is sent,
// the drop policy causes a datagram to be dropped. The dropped datagram
// should be reported with status 'abandoned' (not 'lost'), indicating
// it was never actually sent on the wire.

import { hasQuic, skip, mustCall, mustCallAtLeast } from '../common/index.mjs';
import assert from 'node:assert';

const { notStrictEqual, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

let serverSession;

const serverEndpoint = await listen(mustCall((session) => {
  serverSession = session;
}), {
  transportParams: { maxDatagramFrameSize: 1200 },
});

const ids = [0n, 0n, 0n];
let abandoned = false;

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxDatagramFrameSize: 1200 },
  ondatagramstatus: mustCallAtLeast((id, status) => {
    if (status === 'abandoned') {
      strictEqual(id, ids[0]);
      abandoned = true;
    }
    // We'll likely only get status for one other datagram.
  }),
});
await clientSession.opened;

// Set a very small queue so overflow happens immediately.
clientSession.maxPendingDatagrams = 2;

// Send 3 datagrams with a queue size of 2. The first datagram should
// be abandoned when the third is sent (drop-oldest policy is default).
ids[0] = await clientSession.sendDatagram(new Uint8Array([1]));
ids[1] = await clientSession.sendDatagram(new Uint8Array([2]));
ids[2] = await clientSession.sendDatagram(new Uint8Array([3]));

notStrictEqual(ids[0], 0n);
notStrictEqual(ids[1], 0n);
notStrictEqual(ids[2], 0n);

// The abandoned status fires synchronously during sendDatagram when the
// queue overflows. It should already be set
strictEqual(abandoned, true);

await Promise.all([
  serverSession.close(),
  clientSession.close(),
]);
await serverEndpoint.close();
