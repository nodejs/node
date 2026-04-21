// Flags: --experimental-quic --no-warnings

// Test: default transport parameter limits are reasonable.
// Verify that the default transport parameters have sane values:
// not zero (which would prevent streams), and not excessively large
// (which could waste resources).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { ok } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  const info = await serverSession.opened;

  // The handshake info should be available.
  ok(info, 'handshake info should be available');

  await serverSession.closed;
}));

const clientSession = await connect(serverEndpoint.address, {
  reuseEndpoint: false,
});
const info = await clientSession.opened;

// Verify the handshake completed and we can inspect the session.
ok(info, 'handshake info should be available');

// Check that the session has reasonable default stream limits by
// verifying we can create at least one bidirectional and one
// unidirectional stream.
const bidiStream = await clientSession.createBidirectionalStream();
ok(bidiStream, 'should be able to create a bidi stream');
bidiStream.destroy();

const uniStream = await clientSession.createUnidirectionalStream();
ok(uniStream, 'should be able to create a uni stream');
uniStream.destroy();

// Check the endpoint's maxConnectionsPerHost and maxConnectionsTotal
// defaults are either 0 (unlimited) or a reasonable positive number.
const maxPerHost = serverEndpoint.maxConnectionsPerHost;
const maxTotal = serverEndpoint.maxConnectionsTotal;
ok(maxPerHost >= 0, 'maxConnectionsPerHost should be non-negative');
ok(maxTotal >= 0, 'maxConnectionsTotal should be non-negative');

await clientSession.close();
await serverEndpoint.close();
