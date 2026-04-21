// Flags: --experimental-quic --no-warnings

// Test: SNI mismatch.
// Client connects with a servername that doesn't match any SNI entry
// and no wildcard is configured. The handshake should fail with a
// TLS alert (unrecognized_name).

import { hasQuic, skip, mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import * as fixtures from '../common/fixtures.mjs';

const { rejects, strictEqual } = assert;
const { readKey } = fixtures;

if (!hasQuic) {
  skip('QUIC is not enabled');
}

const { listen, connect } = await import('node:quic');
const { createPrivateKey } = await import('node:crypto');

const key = createPrivateKey(readKey('agent1-key.pem'));
const cert = readKey('agent1-cert.pem');

// Server only has an entry for 'specific.example.com', no wildcard.
// Connections to any other hostname will be rejected at the TLS level.
const serverEndpoint = await listen(mustCall(async (serverSession) => {
  await rejects(serverSession.opened, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  });
  await rejects(serverSession.closed, {
    code: 'ERR_QUIC_TRANSPORT_ERROR',
  });
}), {
  sni: { 'specific.example.com': { keys: [key], certs: [cert] } },
  alpn: ['quic-test'],
  transportParams: { maxIdleTimeout: 1 },
  onerror: mustCall((err) => {
    strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
  }),
});

// Client connects with a different servername — no matching identity.
const clientSession = await connect(serverEndpoint.address, {
  alpn: 'quic-test',
  servername: 'wrong.example.com',
  transportParams: { maxIdleTimeout: 1 },
  onerror: mustCall((err) => {
    strictEqual(err.code, 'ERR_QUIC_TRANSPORT_ERROR');
  }),
});

await rejects(clientSession.opened, {
  code: 'ERR_QUIC_TRANSPORT_ERROR',
});

await rejects(clientSession.closed, {
  code: 'ERR_QUIC_TRANSPORT_ERROR',
});

await serverEndpoint.close();
