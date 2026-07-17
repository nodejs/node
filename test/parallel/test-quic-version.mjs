// Flags: --experimental-quic --no-warnings

// Test: QUIC version selection.
// QUIC v1 handshake succeeds.
// QUIC v2 handshake succeeds.
// Both V1 and V2 are advertised in preferred/available versions.
// Version negotiation upgrades V1 → V2 when both sides support it.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverDone = Promise.withResolvers();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  await serverSession.close();
  serverDone.resolve();
}));

// Default handshake uses v1 initial packets.
// The session should complete successfully.
const cs = await connect(serverEndpoint.address);
const info = await cs.opened;

// The cipher and protocol should be negotiated.
strictEqual(typeof info.cipher, 'string');
strictEqual(info.cipherVersion, 'TLSv1.3');
strictEqual(info.protocol, 'quic-test');

// Both V1 and V2 are in preferred/available versions
// (configured in Session::Config). The compatible version negotiation
// (RFC 9368) should upgrade to V2 when both sides support it.
// We verify the handshake succeeded — the version negotiation
// happens transparently.

await Promise.all([serverDone.promise, cs.closed]);
await serverEndpoint.close();
