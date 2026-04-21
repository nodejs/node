// Flags: --experimental-quic --no-warnings

// Test: operations on destroyed session/stream.
// Operations on a destroyed session return gracefully.
// Operations on a destroyed stream return gracefully.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

const { rejects, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.closed;
}));

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

const stream = await clientSession.createBidirectionalStream();

// Destroy the stream, then try operations on it.
stream.destroy();
strictEqual(stream.destroyed, true);

// Operations on destroyed stream should not throw.
stream.destroy();  // Idempotent.
stream.writer.endSync();  // No-op on destroyed.

// Destroy the session, then try operations on it.
clientSession.destroy();
strictEqual(clientSession.destroyed, true);

// Properties should return null/undefined gracefully.
strictEqual(clientSession.endpoint, null);
strictEqual(clientSession.path, undefined);
strictEqual(clientSession.certificate, undefined);
strictEqual(clientSession.peerCertificate, undefined);
strictEqual(clientSession.ephemeralKeyInfo, undefined);

// destroy() again is idempotent.
clientSession.destroy();

// sendDatagram on destroyed session throws ERR_INVALID_STATE.
await rejects(
  clientSession.sendDatagram(new Uint8Array([1])),
  { code: 'ERR_INVALID_STATE' },
);

await serverEndpoint.close();
