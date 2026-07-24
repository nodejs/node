// Flags: --experimental-quic --no-warnings

// Test: connection total limit enforcement.
// maxConnectionsTotal limits total concurrent connections.
// When the limit is exceeded, the server sends CONNECTION_REFUSED
// and the client's session is destroyed with ERR_QUIC_TRANSPORT_ERROR.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, QuicEndpoint } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
const cert = fixtures.readKey('agent1-cert.pem');

// Create endpoint with maxConnectionsTotal = 1.
const endpoint = new QuicEndpoint({ maxConnectionsTotal: 1 });

// Verify the limits are readable and mutable.
assert.strictEqual(endpoint.maxConnectionsTotal, 1);
// The default maxConnectionsPerHost is 100 — a non-zero default that
// prevents a single host from exhausting server resources.
assert.strictEqual(endpoint.maxConnectionsPerHost, 100);
endpoint.maxConnectionsPerHost = 50;
assert.strictEqual(endpoint.maxConnectionsPerHost, 50);
endpoint.maxConnectionsPerHost = 0;

let sessionCount = 0;

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  sessionCount++;
  await Promise.all([serverSession.opened, serverSession.closed]);
}), {
  endpoint,
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
  transportParams: { maxIdleTimeout: 2 },
});

// First connection should succeed.
const cs1 = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  verifyPeer: 'manual',
  transportParams: { maxIdleTimeout: 2 },
});
await cs1.opened;

// Second connection — server rejects with CONNECTION_REFUSED.
const cs2 = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  verifyPeer: 'manual',
  transportParams: { maxIdleTimeout: 1 },
  onerror: mustCall((err) => {
    assert.strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
  }),
});

await Promise.all([
  assert.rejects(cs2.opened, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  }),
  assert.rejects(cs2.closed, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  }),
]);

// Only 1 session should have been accepted by the server.
assert.strictEqual(sessionCount, 1);

await cs1.close();
await serverEndpoint.close();
