// Flags: --experimental-quic --no-warnings

// Test: opened info includes cert validation error details.
// validationErrorReason populated on cert validation failure.
// validationErrorCode populated on cert validation failure.
// The test helper uses self-signed certs so validation always fails
// (unless rejectUnauthorized is explicitly set).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('../common/quic.mjs');

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  const serverInfo = await serverSession.opened;

  // Server also sees validation info about the peer.
  assert.strictEqual(typeof serverInfo.validationErrorReason, 'string');
  assert.strictEqual(typeof serverInfo.validationErrorCode, 'string');

  serverSession.close();
}));

const clientSession = await connect(serverEndpoint.address, {
  verifyPeer: 'manual',
});
const clientInfo = await clientSession.opened;

// validationErrorReason is a non-empty string describing
// why the cert failed validation (self-signed cert).
assert.strictEqual(typeof clientInfo.validationErrorReason, 'string');
assert.ok(clientInfo.validationErrorReason.length > 0);

// validationErrorCode is the OpenSSL error code string.
assert.strictEqual(typeof clientInfo.validationErrorCode, 'string');
assert.ok(clientInfo.validationErrorCode.length > 0);

await clientSession.closed;
await serverEndpoint.close();
