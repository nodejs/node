// Flags: --experimental-quic --no-warnings

// Test: operations on a closing session.
// createBidirectionalStream on closing session throws.
// sendDatagram on closing session throws.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}), {
  transportParams: { maxDatagramFrameSize: 1200 },
});

const clientSession = await connect(serverEndpoint.address, {
  transportParams: { maxDatagramFrameSize: 1200 },
});
await clientSession.opened;

// Initiate graceful close.
clientSession.close();

// Creating a stream on a closing session rejects.
await rejects(
  clientSession.createBidirectionalStream(),
  { code: 'ERR_INVALID_STATE' },
);

await rejects(
  clientSession.createUnidirectionalStream(),
  { code: 'ERR_INVALID_STATE' },
);

// sendDatagram on a closing session throws.
await rejects(
  clientSession.sendDatagram(new Uint8Array([1, 2, 3])),
  { code: 'ERR_INVALID_STATE' },
);

await clientSession.closed;
await serverEndpoint.close();
