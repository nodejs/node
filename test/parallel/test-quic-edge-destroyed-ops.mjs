// Flags: --experimental-quic --no-warnings

// Test: operations on destroyed session/stream.
// Operations on a destroyed session return gracefully.
// Operations on a destroyed stream return gracefully.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

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
assert.strictEqual(stream.destroyed, true);

// Operations on destroyed stream should not throw.
stream.destroy();  // Idempotent.
stream.writer.endSync();  // No-op on destroyed.

// Destroy the session, then try operations on it.
clientSession.destroy();
assert.strictEqual(clientSession.destroyed, true);

// Properties should return null/undefined gracefully.
assert.strictEqual(clientSession.endpoint, null);
assert.strictEqual(clientSession.path, undefined);
assert.strictEqual(clientSession.certificate, undefined);
assert.strictEqual(clientSession.peerCertificate, undefined);
assert.strictEqual(clientSession.ephemeralKeyInfo, undefined);

// destroy() again is idempotent.
clientSession.destroy();

// sendDatagram on destroyed session throws ERR_INVALID_STATE.
await assert.rejects(
  clientSession.sendDatagram(new Uint8Array([1])),
  { code: 'ERR_INVALID_STATE' },
);

await serverEndpoint.close();
