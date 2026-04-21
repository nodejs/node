// Flags: --experimental-quic --no-warnings

// Test: endpoint.busy rejects new sessions.
// When the busy flag is set, the server sends CONNECTION_REFUSED
// for new connection attempts. Existing sessions are not affected.

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { rejects, strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect, QuicEndpoint } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

const endpoint = new QuicEndpoint();

const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await serverSession.opened;
  await serverSession.closed;
}), {
  endpoint,
  sni: { '*': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
  transportParams: { maxIdleTimeout: 2 },
});

// First connection before busy — should succeed.
const cs1 = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  transportParams: { maxIdleTimeout: 2 },
});
await cs1.opened;

// Set the endpoint busy.
strictEqual(endpoint.busy, false);
endpoint.busy = true;
strictEqual(endpoint.busy, true);

// Second connection while busy — server rejects.
const cs2 = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  transportParams: { maxIdleTimeout: 1 },
  onerror: mustCall((err) => {
    strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
  }),
});

await rejects(cs2.opened, {
  code: 'ERR_QUIC_TRANSPORT_ERROR',
});

await rejects(cs2.closed, {
  code: 'ERR_QUIC_TRANSPORT_ERROR',
});

// Unset busy.
endpoint.busy = false;
strictEqual(endpoint.busy, false);

// Clean up.
await cs1.close();
await serverEndpoint.close();
