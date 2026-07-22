// Flags: --experimental-quic --no-warnings

// Test: diagnostics_channel session events.
// quic.session.handshake fires when handshake completes.
// quic.session.update.key fires on key update.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import dc from 'node:diagnostics_channel';

const { ok, strictEqual } = assert;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

// quic.session.handshake fires on both sides.
let handshakeCount = 0;
dc.subscribe('quic.session.handshake', mustCall((msg) => {
  handshakeCount++;
  ok(msg.session);
  // The handshake info should include standard TLS fields.
  strictEqual(typeof msg.protocol, 'string');
  strictEqual(typeof msg.servername, 'string');
}, 2));

// quic.session.update.key fires on key update.
dc.subscribe('quic.session.update.key', mustCall((msg) => {
  ok(msg.session);
}));

const serverEndpoint = await listen(async (serverSession) => {
  await serverSession.opened;
  await serverSession.close();
});

const clientSession = await connect(serverEndpoint.address);
await clientSession.opened;

// Trigger a key update to fire a key update event.
clientSession.updateKey();

await clientSession.closed;
await serverEndpoint.close();

// Both client and server handshakes should have fired.
strictEqual(handshakeCount, 2);
